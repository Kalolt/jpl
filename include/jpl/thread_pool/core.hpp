#ifndef JPL_THREAD_POOL_CORE_HPP
#define JPL_THREAD_POOL_CORE_HPP

#include <bits/stdint-uintn.h>
#include <cstdint>
#include <jpl/bits/memory_pool.hpp>
#include <jpl/bits/file_data.hpp>
#include <jpl/vector.hpp>
#include <jpl/function.hpp>

#include <boost/context/fiber.hpp>
#include <emmintrin.h>

#include <atomic>
#include <chrono>
#include <type_traits>

#ifdef WIN32
#define constinit
#endif

namespace jpl {

namespace tp {
using clock = ::std::chrono::steady_clock;
class handle;

struct fiber {
	::boost::context::fiber f;
	void* msg;
	friend class tp::handle;

	template<class Fn, class ... Args>
	fiber(Fn&& fn, Args&& ... args) requires ::std::is_invocable_v<Fn, fiber&, Args...>
		: f{ [this, fn = ::std::forward<Fn>(fn), ...args = ::std::forward<Args>(args)](::boost::context::fiber&& sink) mutable {
			f = ::std::move(sink);
			try {
				fn(*this, ::std::forward<Args>(args)...);
			} catch (const ::boost::context::detail::forced_unwind&) {
				throw;
			} catch (...) {
				handle_exception();
			}
			return ::std::move(f);
		} },
		msg{ nullptr }
	{}
	~fiber() noexcept = default;

	fiber(fiber&& other) = delete;
	fiber& operator=(fiber&& other) = delete;
	fiber(const fiber& other) = delete;
	fiber& operator=(const fiber& other) = delete;

	void yield() /* can throw */ {
		f = ::std::move(f).resume();
	}
	bool try_yield(); // can throw
	void yield_or_wait(tp::clock::duration duration);

	void handle_exception() noexcept;
	#ifdef __linux
	::jpl::file_data read_file(const char* path);
	#else
	::jpl::vector<::std::byte> read_file(const char* path);
	#endif
	void wait_until(tp::clock::time_point until);
	void wait_for(tp::clock::duration duration) {
		wait_until(tp::clock::now() + duration);
	}
};

class handle {
	void* ptr;

	public:
	handle(fiber* new_ptr) noexcept {
		uintptr_t ptr_data;
		::memcpy(&ptr_data, &new_ptr, sizeof(void*));
		ptr_data |= 1;
		::memcpy(&ptr, &ptr_data, sizeof(void*));
	};

	template<class Fn, class ... Args> requires ::std::is_invocable_v<Fn, fiber&, Args...>
	handle(Fn&& fn, Args&& ... args) : ptr{ new fiber{ ::std::forward<Fn>(fn), ::std::forward<Args>(args)... } } {
		uintptr_t ptr_data;
		::memcpy(&ptr_data, &ptr, sizeof(void*));
		ptr_data |= 1;
		::memcpy(&ptr, &ptr_data, sizeof(void*));
	}

	template<class Fn, class ... Args> requires ::std::is_invocable_v<Fn, Args...>
	handle(Fn&& fn, Args&& ... args) : ptr{ new jpl::function<void()>{
		[fn = ::std::forward<Fn>(fn), ...args = ::std::forward<Args>(args)]{ fn(::std::forward<Args>(args)...); }
	} } {}

	handle(handle&& other) noexcept {
		ptr = other.ptr;
		other.ptr = nullptr;
	}
	handle& operator=(handle&& other) noexcept {
		ptr = other.ptr;
		other.ptr = nullptr;
		return *this;
	}

	handle(const handle&) = delete;
	handle& operator=(const handle&) = delete;

	~handle() noexcept {
		if (has_fiber())
			delete static_cast<fiber*>(get_fiber());
		else if (ptr)
			delete static_cast<jpl::function<void()>*>(ptr);
	}

	handle resume() &&;
	handle process_msg() &&;

	bool has_fiber() const noexcept {
		return reinterpret_cast<uintptr_t>(ptr) & 1;
	}

	fiber* get_fiber() const noexcept {
		return (fiber*)((uintptr_t)ptr & ~1);
	}

	operator bool() const noexcept {
		if (!ptr)
			return false;
		else
			return !has_fiber() || static_cast<bool>(get_fiber()->f);
	}
};

void init(::size_t n_threads = 0); // can throw
void enqueue(handle&& f);
void enqueue(clock::time_point when, handle&& f);
inline void enqueue(clock::duration duration, handle&& f) {
	enqueue(clock::now() + duration, static_cast<handle&&>(f));
}
void reset();
void join();
void join_and_reset();
void join_and_reset(void(*task)(), clock::duration interval);
void kill() noexcept;
void shut_down();
::size_t size() noexcept;

class mutex final : private ::std::atomic_flag {
	void lock(fiber& f) /* yield can throw */ {
		while (test_and_set(::std::memory_order_acquire))
			f.try_yield();
	}
	void lock() noexcept {
		// TODO: replace this with C++20's atomic_flag::wait when compilers support it!
		while (test_and_set(::std::memory_order_acquire))
			_mm_pause();
	}
	void unlock() noexcept {
		clear(::std::memory_order_release);
	}
	friend class lock;
};

inline constexpr struct sync_token {} sync; 
class lock {
	mutex* atom;
	public:
	[[nodiscard]] lock(fiber& f, mutex* atom) /* can throw */ : atom{ atom } {
		atom->lock(f);
	}
	// Require an explicit token for synchronized locking to avoid accidents
	[[nodiscard]] lock(sync_token, mutex* atom) noexcept : atom{ atom } {
		atom->lock();
	}
	~lock() noexcept {
		if (atom) atom->unlock();
	}

	lock(lock&& other) noexcept : atom{ other.atom } {
		other.atom = nullptr;
	};
	lock& operator=(lock&& other) noexcept {
		mutex* temp = atom;
		atom = other.atom;
		other.atom = temp;
		return *this;
	};

	lock(const lock&) = delete;
	lock& operator=(const lock&) = delete;

	void unlock() noexcept {
		if (atom) {
			atom->unlock();
			atom = nullptr;
		}
	}
};

} // namespace tp

} // namespace jpl

#endif // JPL_THREAD_POOL_CORE_HPP

#ifdef JPL_HEADER_ONLY
#ifndef JPL_THREAD_POOL_IMPL
#define JPL_THREAD_POOL_IMPL
#include <jpl/src/thread_pool/core.cpp>
#endif
#endif
