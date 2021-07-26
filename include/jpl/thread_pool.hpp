#ifndef JPL_THREAD_POOL_CORE_HPP
#define JPL_THREAD_POOL_CORE_HPP

#include <jpl/vector.hpp>
#include <jpl/bits/thread_pool/task.hpp>

#if __has_include(<coroutine>)
#include <coroutine>
#elif __has_include(<experimental/coroutine>)
#include <experimental/coroutine>
namespace std { using namespace experimental; }
#else
#error "requires C++20 coroutines"
#endif
#include <chrono>

namespace jpl::tp {

using clock = ::std::chrono::steady_clock;

struct handle { ~handle(); };
[[nodiscard]] handle init(::size_t n_threads = 0);
void join() noexcept;

inline void enqueue(task&& t) noexcept;
inline void enqueue(auto&& func, auto&& ... args) noexcept {
	if constexpr (sizeof...(args) > 0) {
		enqueue(task{
			[func = static_cast<decltype(func)&&>(func), ...args = static_cast<decltype(args)&&>(args)]() mutable {
				static_cast<decltype(func)&&>(func)(static_cast<decltype(args)&&>(args)...);
			}
		});
	} else {
		enqueue(task{ static_cast<decltype(func)&&>(func) });
	}
}

struct try_yield {
	const bool resumed;
	try_yield() noexcept;
	bool await_ready() const noexcept { return !resumed; }
	void await_suspend(::std::coroutine_handle<> handle) noexcept;
	bool await_resume() const noexcept { return resumed; }
};

struct yield {
	static constexpr bool await_ready() noexcept { return false; }
	void await_suspend(::std::coroutine_handle<> handle) noexcept;
	static constexpr void await_resume() noexcept {}
};

struct sleep_for {
	clock::duration duration;
	static constexpr bool await_ready() noexcept { return false; }
	void await_suspend(::std::coroutine_handle<> handle) noexcept;
	static constexpr void await_resume() noexcept {}
};

struct sleep_until {
	clock::time_point ts;
	bool await_ready() const noexcept { return clock::now() > ts; }
	void await_suspend(::std::coroutine_handle<> handle) noexcept;
	static constexpr void await_resume() noexcept {}
};

struct read_file_awaiter {
	int fd;
	::jpl::vector<char> buffer;
	read_file_awaiter(int fd);
	bool await_ready() noexcept { return fd < 0; }
	void await_suspend(::std::coroutine_handle<> handle);
	::jpl::vector<char>&& await_resume() noexcept {
		return static_cast<::jpl::vector<char>&&>(buffer);
	}
};
read_file_awaiter read_file(const char* file_path);

void process_timed();

} // namespace jpl::tp

#ifdef JPL_HEADER_ONLY
#ifndef JPL_THREAD_POOL_IMPL
#define JPL_THREAD_POOL_IMPL
#include <jpl/src/thread_pool_v2/core.cpp>
#endif
#endif

#endif // JPL_THREAD_POOL_CORE_HPP
