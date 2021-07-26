#ifndef JPL_CONCURRENT_QUEUE_HPP
#define JPL_CONCURRENT_QUEUE_HPP

#include <atomic>
#include <bit>
#include <climits>
#include <cstddef>
#include <new>
#include <optional>
#include <type_traits>

#ifdef __linux__
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
#elif defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#include "synchapi.h"
#else
#error "jpl::concurrent_queue is only supported on Linux and Windows"
#endif

namespace jpl {

#ifdef __cpp_lib_hardware_interference_size
	using ::std::hardware_destructive_interference_size;
#elif defined(JPL_CACHE_LINE_SIZE)
	inline constexpr ::size_t hardware_destructive_interference_size{ JPL_CACHE_LINE_SIZE };
#else
	inline constexpr ::size_t hardware_destructive_interference_size{ 64 };
#endif

template<class T, ::uint32_t ring_buffer_size, bool use_optional = true
#ifdef JPL_CONCURRENT_QUEUE_TEST_OFFSET
	, ::uint32_t offset = 0
#endif
>
	requires(
		// buffer size must be a power of 2, because it guarantees that the indices are continuous when counters overflow
		// it also allows the compiler to optimize modulo operations into bitwise ANDs
		(::std::popcount(ring_buffer_size) == 1)
		// To ensure integrity of the queue, all operations must be noexcept, which means that the stored type must be
		// noexcept movable and destructible.
		&& ::std::is_nothrow_move_constructible_v<T>
		&& ::std::is_nothrow_destructible_v<T>
	)
class concurrent_queue {
	struct node {
		union storage_t {
			T val;
			storage_t() noexcept {}
			~storage_t() noexcept {}
		};
		storage_t storage;
		::std::atomic<::uint32_t> waiters;
		::std::atomic<::uint32_t> state;
	};

	#ifndef JPL_CONCURRENT_QUEUE_TEST_OFFSET
	static constexpr ::uint32_t offset{ 0 };
	#endif

	using optional_t = std::conditional_t<use_optional, ::std::optional<T>, T>;

	// Shuffle indices when accessing ring buffer to avoid false sharing, while still trying to benefit from true sharing.
	[[gnu::always_inline]] static constexpr ::uint32_t shuffle_idx(::uint32_t idx) noexcept {
		constexpr ::uint32_t per_cache_line = hardware_destructive_interference_size / alignof(node);
		constexpr ::uint32_t repeat_after = 8 < per_cache_line ? 8 : per_cache_line;
		constexpr ::uint32_t period = per_cache_line * repeat_after;
		// assuming for example per_cache_line = 16 and repeat_after = 2, this will lead to a pattern like
		// 0, 16, 1, 17, 2, 18, 3, 19, etc.
		// eventually it will go for example from 31 to 32, but those should be on different cache lines, because
		// the ring buffer is aligned to hardware_destructive_interference_size, and the period is a multiple of cache line size
		if constexpr (per_cache_line < 2)
			return idx;
		else
			return (idx / period * period) + ((idx / repeat_after) % per_cache_line) + ((idx % repeat_after) * per_cache_line);
	}

	alignas(hardware_destructive_interference_size) node buffer[ring_buffer_size];
	alignas(hardware_destructive_interference_size) ::std::atomic<::uint32_t> head{ offset };
	alignas(hardware_destructive_interference_size) ::std::atomic<::uint32_t> tail{ offset };

	public:
	concurrent_queue() noexcept {
		for (::uint32_t i = 0; i != ring_buffer_size; ++i) {
			if constexpr (offset)
				buffer[shuffle_idx((i + offset) % ring_buffer_size)].state.store(i + offset, ::std::memory_order::relaxed);
			else
				buffer[shuffle_idx(i)].state.store(i, ::std::memory_order::relaxed);
		}
	}
	~concurrent_queue() noexcept {
		while (head != tail) [[unlikely]] {
			::uint32_t idx = shuffle_idx(head++ % ring_buffer_size);
			(buffer[idx].storage.val).~T();
		}
	};

	concurrent_queue(const concurrent_queue&) = delete;
	concurrent_queue& operator=(const concurrent_queue&) = delete;
	concurrent_queue(concurrent_queue&&) = delete;
	concurrent_queue& operator=(concurrent_queue&&) = delete;

	private:
	[[gnu::always_inline]] static void wait(node& n, ::uint32_t val) noexcept {
		n.waiters++;
		#ifdef __linux__
		::syscall(SYS_futex, &n.state, FUTEX_WAIT_PRIVATE, val, nullptr, nullptr, 0);
		#elif defined(_MSC_VER)
		::WaitOnAddress(&n.state, &val, 4, INFINITE);
		#endif
		n.waiters--;
	}

	[[gnu::always_inline]] static void notify_all(node& n) noexcept {
		if (!n.waiters) return;
		#ifdef __linux__
		::syscall(SYS_futex, &n.state, FUTEX_WAKE_PRIVATE, INT_MAX, nullptr, nullptr, 0);
		#elif defined(_MSC_VER)
		::WakeByAddressAll(&n.state);
		#endif
	}

	template<class U>
	[[gnu::always_inline]] void push_impl(U&& val, ::uint32_t turn_number) noexcept {
		const ::uint32_t idx = shuffle_idx(turn_number % ring_buffer_size);
		::uint32_t state_val = buffer[idx].state.load(::std::memory_order::acquire);
		// Mark as unlikely, because this will only wait if ring buffer is full.
		while (state_val != turn_number) [[unlikely]] {
			wait(buffer[idx], state_val);
			state_val = buffer[idx].state.load(::std::memory_order::acquire);
		}
		::new (&buffer[idx].storage.val) T{ static_cast<U&&>(val) };

		buffer[idx].state.store(turn_number + 1);
		notify_all(buffer[idx]);
	}

	public:
	[[nodiscard]] T pop() noexcept {
		const ::uint32_t turn_number = head.fetch_add(1, ::std::memory_order::acquire);
		const ::uint32_t idx = shuffle_idx(turn_number % ring_buffer_size);

		::uint32_t state_val = buffer[idx].state.load(::std::memory_order::acquire);
		while (state_val != uint32_t(turn_number + 1)) {
			wait(buffer[idx], state_val);
			state_val = buffer[idx].state.load(::std::memory_order::acquire);
		}

		T val{ static_cast<T&&>(buffer[idx].storage.val) };
		(buffer[idx].storage.val).~T();

		buffer[idx].state.store(turn_number + ring_buffer_size);
		notify_all(buffer[idx]);

		return val;
	}

	[[nodiscard]] optional_t try_pop() noexcept {
		::uint32_t turn_number = head.load(::std::memory_order::acquire);
		::uint32_t idx;
		do {
			idx = shuffle_idx(turn_number % ring_buffer_size);
			const ::uint32_t state = buffer[idx].state.load();
			if ((state != uint32_t(turn_number + 1)))
				return {};
		} while (!head.compare_exchange_weak(turn_number, turn_number + 1));

		optional_t val{ static_cast<T&&>(buffer[idx].storage.val) };
		(buffer[idx].storage.val).~T();

		buffer[idx].state.store(turn_number + ring_buffer_size);
		notify_all(buffer[idx]);

		return val;
	}

	[[nodiscard]] bool try_pop(T& out) noexcept requires(::std::is_nothrow_move_assignable_v<T>) {
		::uint32_t turn_number = head.load(::std::memory_order::acquire);
		::uint32_t idx;
		do {
			idx = shuffle_idx(turn_number % ring_buffer_size);
			const ::uint32_t state = buffer[idx].state.load();
			if ((state != uint32_t(turn_number + 1)))
				return false;
		} while (!head.compare_exchange_weak(turn_number, turn_number + 1));

		out = static_cast<T&&>(buffer[idx].storage.val);
		(buffer[idx].storage.val).~T();

		buffer[idx].state.store(turn_number + ring_buffer_size);
		notify_all(buffer[idx]);

		return true;
	}

	void push(const T& val) noexcept requires(::std::is_nothrow_copy_constructible_v<T>) {
		const ::uint32_t turn_number = tail.fetch_add(1, ::std::memory_order::acquire);
		push_impl(val, turn_number);
	}

	void push(T&& val) noexcept {
		const ::uint32_t turn_number = tail.fetch_add(1, ::std::memory_order::acquire);
		push_impl(static_cast<T&&>(val), turn_number);
	}

	bool try_push(const T& val) noexcept requires(::std::is_nothrow_copy_constructible_v<T>) {
		::uint32_t turn_number = tail.load(::std::memory_order::acquire);
		do {
			if ((turn_number - head.load()) >= ring_buffer_size)
				return false;
		} while (!tail.compare_exchange_weak(turn_number, turn_number + 1));
		push_impl(val, turn_number);
		return true;
	}

	bool try_push(T&& val) noexcept {
		::uint32_t turn_number = tail.load(::std::memory_order::acquire);
		do {
			if ((turn_number - head.load()) >= ring_buffer_size)
				return false;
		} while (!tail.compare_exchange_weak(turn_number, turn_number + 1));
		push_impl(static_cast<T&&>(val), turn_number);
		return true;
	}
};

} // namespace jpl

#endif // JPL_CONCURRENT_QUEUE_HPP
