#ifndef JPL_THREAD_POOL_TASK_HPP
#define JPL_THREAD_POOL_TASK_HPP

#include <atomic>
#include <chrono>
#include <cstdint>
#include <type_traits>

namespace jpl::tp {

using clock = std::chrono::steady_clock;

namespace detail { inline ::std::atomic<::uint32_t> pending_tasks; }

class task {
	static constexpr ::size_t buffer_size = 40;

	void(*invoker)(void*) noexcept(false);
	void(*dtor)   (void*) noexcept;
	alignas(8) char storage[buffer_size];

	public:
	task() noexcept
		: invoker{ nullptr }
	{}

	template<class T>
		requires( bool(
			::std::is_trivially_copy_constructible_v<::std::decay_t<T>> &&
			::std::is_trivially_destructible_v<::std::decay_t<T>> &&
			(sizeof(::std::decay_t<T>) <= buffer_size) &&
			(alignof(::std::decay_t<T>) <= 8)
		) )
	task(T callable) noexcept {
		using type = ::std::decay_t<T>;
		detail::pending_tasks++;
		::memcpy(storage, &callable, sizeof(type));
		invoker = +[](void* storage) noexcept(false) {
			type& callable{ *reinterpret_cast<type*>(storage) }; 
			try {
				callable();
				detail::pending_tasks--;
			} catch (...) {
				detail::pending_tasks--;
				throw;
			}
		};
		dtor = nullptr;
	}

	template<class T>
		requires( bool(
			(!::std::is_trivially_copy_constructible_v<::std::decay_t<T>>) ||
			(!::std::is_trivially_destructible_v<::std::decay_t<T>>) ||
			(sizeof(::std::decay_t<T>) > buffer_size)
		) )
	task(T&& callable) {
		using type = ::std::decay_t<T>;
		detail::pending_tasks++;
		new (storage) type*{ new type{ static_cast<T&&>(callable) } };
		invoker = +[](void* storage) noexcept(false) {
			type* ptr;
			::memcpy(&ptr, storage, 8);
			try {
				(*ptr)();
				detail::pending_tasks--;
			} catch (...) {
				detail::pending_tasks--;
				throw;
			}
		};
		dtor = +[](void* storage) noexcept {
			type* ptr;
			::memcpy(&ptr, storage, 8);
			delete ptr;
		};
	}

	task (task&& other) noexcept {
		::memcpy(this, &other, sizeof(task));
		other.invoker = nullptr;
	}
	task& operator=(task&& other) noexcept {
		::memcpy(this, &other, sizeof(task));
		other.invoker = nullptr;
		return *this;
	}
	~task() noexcept {
		if (dtor && invoker) [[unlikely]]
			dtor(storage);
	}

	operator bool() const noexcept {
		return invoker;
	}

	void operator()() {
		invoker(storage);
	}
};

struct timed_task {
	task t;
	clock::time_point queue_at;

	bool operator<(const timed_task& other) const noexcept {
		return queue_at < other.queue_at;
	}
};

} // namespace jpl::tp

#endif // JPL_THREAD_POOL_TASK_HPP
