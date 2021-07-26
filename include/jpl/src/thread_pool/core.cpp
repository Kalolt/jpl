#include <jpl/thread_pool.hpp>
#include <jpl/concurrent_queue.hpp>
#include <jpl/vector.hpp>
#include <jpl/bits/thread_pool/io.hpp>

#include <thread>
#include <queue>
#include <mutex>

#include <fmt/format.h>

namespace jpl::tp {

using namespace ::std::chrono_literals;

constexpr ::size_t n_timer_threads{ 2 };

inline thread_local task try_task;
// TODO: the ring buffer size should be configurable, and there probably should be a dynamic overflow buffer for when it's full
inline ::jpl::concurrent_queue<task, 2048, false> task_queue;
inline ::jpl::concurrent_queue<task, 1024, false> ready_timed_events;
inline ::std::atomic<bool> quit{ false };
inline ::std::mutex timed_task_mutex;
inline ::std::priority_queue<timed_task> timed_tasks;
inline ::jpl::vector<::std::thread> threads;
inline ::jpl::vector<::std::thread, n_timer_threads> timer_threads;

inline void process_timed() {
	::std::lock_guard lock{ timed_task_mutex };
	while (!timed_tasks.empty() && (timed_tasks.top().queue_at < clock::now())) {
		ready_timed_events.push(static_cast<task&&>(const_cast<task&>(timed_tasks.top().t)));
		timed_tasks.pop();
	}
}

inline void join() noexcept {
	clock::duration sleep_duration{ 5ms };
	while (detail::pending_tasks && !quit) {
		process_io(sleep_duration);
		process_timed();
		if (!timed_tasks.empty()) {
			sleep_duration = ::std::min<clock::duration>(5ms, timed_tasks.top().queue_at - clock::now());
		}
	}
}

inline void cleanup() noexcept {
	quit = true;
	for (::size_t i = 0; i != threads.size(); ++i)
		task_queue.push([]{});
	for (::size_t i = 0; i != timer_threads.size(); ++i)
		ready_timed_events.push([]{});
	for (auto& t : threads) t.join();
	for (auto& t : timer_threads) t.join();
	free_io();
}

handle::~handle() {
	join();
	cleanup();
}

template<auto& event_source>
inline void task_loop() {
	try {
		while (!quit) {
			event_source.pop()();
			while (try_task) {
				task t = static_cast<task&&>(try_task);
				t();
			}
		}
	} catch (const ::std::exception& err) {
		// TODO: do something more reasonable here
		::fmt::print("Caught unhandled exception! | {}\n", err.what());
		quit = true;
	} catch (...) {
		::fmt::print("Caught unhandled exception of unknown type!\n");
		quit = true;
	}
}

inline handle init(::size_t n_threads) {
	init_io();
	n_threads = n_threads ? n_threads : ::std::thread::hardware_concurrency();
	threads.reserve(n_threads);
	try {
		for (::size_t i = 0; i != n_threads      ; ++i) threads      .emplace_back(task_loop<task_queue>);
		for (::size_t i = 0; i != n_timer_threads; ++i) timer_threads.emplace_back(task_loop<ready_timed_events>);
	} catch (...) {
		cleanup();
		throw;
	}
	return {};
}

inline void enqueue(task&& t) noexcept {
	task_queue.push(static_cast<task&&>(t));
}

try_yield::try_yield() noexcept : resumed{ task_queue.try_pop(try_task) } {}

void try_yield::await_suspend(::std::coroutine_handle<> handle) noexcept {
	task_queue.push(handle);
}

void yield::await_suspend(::std::coroutine_handle<> handle) noexcept {
	task_queue.push(handle);
}

void sleep_for::await_suspend(::std::coroutine_handle<> handle) noexcept {
	::std::lock_guard lock{ timed_task_mutex };
	timed_tasks.emplace(timed_task{ handle, clock::now() + duration });
}

void sleep_until::await_suspend(::std::coroutine_handle<> handle) noexcept {
	::std::lock_guard lock{ timed_task_mutex };
	timed_tasks.emplace(timed_task{ handle, ts });
}

} // namespace jpl::tp
