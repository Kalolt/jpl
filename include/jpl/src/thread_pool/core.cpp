/*
	TODO:
		-Considering using a ring buffer for queueing tasks
		-Figure out why io_uring seems to require to be initialized with higher queue size than is actually used
		-Handle file io in a dedicated thread.
		-Reuse io related objects on Windows
		-Deal with max file handle limit somehow on Windows
		-If msg was thread_local, it could allow for greatly simplified code
		-Consider whether mutex contention is an issue
			-When running lots of small tasks, synchronization overhead can be significant
			-There could be 2 or more queues with their own mutexes, with groups of threads preferring their own
*/

#include <jpl/thread_pool/core.hpp>

#ifdef __linux
#include <liburing.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>	
#else
#define NOMINMAX
#define WIN32_MEAN_AND_LEAN
#include <windows.h>
#include <iostream>
#endif

#include <algorithm>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>

namespace jpl::tp {

using namespace ::std::chrono_literals;

enum class msg_type {
	read_file,
	wait_until,
	exception,
};

struct read_file_msg {
	msg_type type;
	const char* path;
	#ifdef __linux
	::jpl::file_data* data;
	int fd;
	#else
	HANDLE fd;
	HANDLE event;
	#endif
	fiber* task;
	read_file_msg* next{ nullptr };
};

struct wait_until_msg {
	msg_type type;
	clock::time_point when;
};

static constexpr struct {
	msg_type type{ msg_type::exception };
} exception_msg;

struct timed_task {
	tp::handle task;
	clock::time_point start_at;

	bool operator<(const timed_task& other) const noexcept {
		return start_at < other.start_at;
	}
};

static ::std::atomic<bool> quit{ false };
static ::std::atomic<::uint64_t> n_tasks{ 0 };
static ::std::mutex mtx;
static ::std::mutex delayed_mtx;
static ::std::queue<handle> tasks;
static ::jpl::vector<::std::exception_ptr> exceptions;

static ::std::priority_queue<timed_task> timed_tasks;
static clock::time_point next_timed_task;

static ::std::condition_variable cv;
static ::jpl::vector<::std::thread> threads;

static int pending_reads{ 0 };
#ifdef __linux
	static ::std::mutex uring_mtx;
	static read_file_msg* read_queue;
	static constexpr int n_uring_entries{ 16 };
	static ::io_uring ring;
	static const int page_size{ ::getpagesize() };
#else // Windows
	static struct {
		::jpl::vector<::HANDLE> events;
		::jpl::vector<read_file_msg*> msg;
		read_file_msg* read_queue{ nullptr };
		::std::mutex io_mtx;

		void queue(read_file_msg* msg) {
			::std::lock_guard lock{ io_mtx };
			msg->next = read_queue;
			read_queue = msg;
		}

		bool process() {
			::std::unique_lock lock{ io_mtx };
			while (read_queue && (pending_reads < MAXIMUM_WAIT_OBJECTS)) {
				read_file_msg* msg = read_queue;
				read_queue = read_queue->next;
				lock.unlock();
				io_events.add_event(msg->event, static_cast<read_file_msg*>(msg));
				pending_reads++;
				lock.lock();
			}

			if (!pending_reads) return false;

			::uint32_t res = ::WaitForMultipleObjects(events.size(), events.data(), false, 1);
			if (res == WAIT_FAILED) {
				::LPWSTR buffer = nullptr;
				::FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, nullptr, ::GetLastError(), 0, (::LPWSTR)&buffer, 0, nullptr);
				::std::wcout << buffer << ::std::endl;
			}
			//assert(res != WAIT_FAILED);
			if (res != WAIT_TIMEOUT && res != WAIT_FAILED) {
				::uint32_t idx = res - WAIT_OBJECT_0;
				{
					::std::lock_guard lock{ mtx };
					tasks.emplace(msg[idx]->task);
				}
				cv.notify_one();
				::CloseHandle(msg[idx]->fd);
				::delete msg[idx];
				pending_reads--;
				::CloseHandle(events[idx]);
				erase(idx);
			}

			return true;
		}

		private:
		void add_event(::HANDLE event, read_file_msg* new_msg) {
			events.push_back(event);
			msg.push_back(new_msg);
		}

		void erase(int idx) {
			events.erase(events.begin() + idx);
			msg.erase(msg.begin() + idx);
		}
	} io_events;
#endif

static handle pop_queue(::std::queue<handle>& queue) noexcept {
	handle task{ ::std::move(queue.front()) };
	queue.pop();
	return task;
}

static handle pop_queue(::std::priority_queue<timed_task>& queue) noexcept {
	handle task{ ::std::move(const_cast<handle&>(queue.top().task)) };
	queue.pop();
	return task;
}

static bool run_task() {
	::std::unique_lock lock{ mtx };
	if (tasks.empty())
		return false;
	handle task{ pop_queue(tasks) };
	lock.unlock();
	task = ::std::move(task).resume();
	if (task) {
		lock.lock();
		tasks.push(::std::move(task));
	} else {
		n_tasks--;
	}
	return true;
}

static void loop() {
	while (!quit.load(::std::memory_order_relaxed)) {
		if (!run_task()) {
			::std::unique_lock lock{ mtx };
			cv.wait(lock, []{ return quit.load(::std::memory_order_relaxed) || !tasks.empty(); });
		}
	}
}

void init(::size_t n_threads) /* can throw */ {
	#ifdef __linux
	[[maybe_unused]] int res = ::io_uring_queue_init(n_uring_entries * 8, &ring, 0);
	assert(res == 0);
	#else
	#endif
	n_threads = n_threads ? n_threads : ::std::thread::hardware_concurrency();
	exceptions.reserve(::jpl::sic, n_threads);
	threads.resize(n_threads, []{
		return ::std::thread{ loop };
	});
}

void enqueue(handle&& f) {
	n_tasks++;
	{
		::std::lock_guard lock{ mtx };
		tasks.emplace(static_cast<handle&&>(f));
	}
	cv.notify_one();
}

void enqueue(clock::time_point when, handle&& f) {
	n_tasks++;
	::std::lock_guard lock{ delayed_mtx };
	timed_tasks.emplace(timed_task{ static_cast<handle&&>(f), when });
	next_timed_task = ::std::min(when, next_timed_task);
}

void reset() {
	quit = true;
	cv.notify_all();
	for (auto& thread : threads) thread.join();
	if (!exceptions.empty())
		std::rethrow_exception(exceptions.front());
	decltype(tasks){}.swap(tasks);
	decltype(timed_tasks){}.swap(timed_tasks);
	next_timed_task = clock::time_point::max();
	quit = false;
	n_tasks = 0;
	for (auto& thread : threads)
		thread = ::std::thread{ loop };
}

static clock::time_point process_timed(clock::time_point now) {
	::std::lock_guard lock{ delayed_mtx };
	if (now > next_timed_task) {
		while (!timed_tasks.empty() && (now >= timed_tasks.top().start_at)) {
			handle f{ pop_queue(timed_tasks) };
			{
				::std::lock_guard lock{ mtx };
				tasks.push(::std::move(f));
			}
			cv.notify_one();
		}
		if (timed_tasks.empty())
			next_timed_task = clock::time_point::max();
		else
			next_timed_task = timed_tasks.top().start_at;
	}
	return next_timed_task;
}

static void process_io_and_wait(clock::time_point until) {
	#ifdef __linux
		{
			::std::unique_lock lock{ uring_mtx };
			bool added_new = false;
			while (read_queue && (pending_reads < (n_uring_entries))) {
				read_file_msg* msg = read_queue;
				read_queue = read_queue->next;
				lock.unlock();
				::io_uring_sqe* sqe = ::io_uring_get_sqe(&ring);
				assert(sqe);
				msg->fd = ::open(msg->path, O_RDONLY);
				::io_uring_prep_read(sqe, msg->fd, msg->data->data_, msg->data->size_, 0);
				::io_uring_sqe_set_data(sqe, msg);
				pending_reads++;
				added_new = true;
				lock.lock();
			}
			if (added_new) {
				[[maybe_unused]] int res = ::io_uring_submit(&ring);
				assert(res);	
			}
		}

		if (pending_reads) {
			//fmt::print("BEG Pending reads: {}\n", pending_reads);
			int res;
			clock::time_point now = clock::now();
			do {
				::__kernel_timespec timeout{ .tv_nsec = ::std::chrono::duration_cast<::std::chrono::nanoseconds>(until - now).count() };
				::io_uring_cqe *cqe;
				//io_uring_peek_batch_cqe
				res = ::io_uring_wait_cqe_timeout(&ring, &cqe, &timeout);
				//res = ::io_uring_wait_cqe(&ring, &cqe);
				if (res != -ETIME) {
					if (res != 0) {
						if (res == -EBUSY) {
							::std::this_thread::sleep_until(until);
							return;
						}
						assert(false && "Unexpected result from io_uring_wait_cqe_timeout");
					}
					read_file_msg* msg = static_cast<read_file_msg*>(::io_uring_cqe_get_data(cqe));
					{
						::std::lock_guard lock{ mtx };
						tasks.emplace(msg->task);
					}
					cv.notify_one();
					::close(msg->fd);
					::delete static_cast<read_file_msg*>(msg);
					pending_reads--;
					::io_uring_cqe_seen(&ring, cqe);
				}
				now = clock::now();
				if (now >= until)
					res = -ETIME;
			} while ((res != -ETIME) && pending_reads);
			//fmt::print("END Pending reads: {}\n", pending_reads);
		} else {
			::std::this_thread::sleep_until(until);
		}
	#else
		if (!io_events.process())
			::std::this_thread::sleep_until(until);
	#endif
}

void join() {
	while (n_tasks && !quit.load(::std::memory_order_acquire)) {
		const clock::time_point now{ clock::now() };
		const clock::time_point next{ process_timed(now) };
		process_io_and_wait(::std::min(now + 100us, next));
	}
}

void join_and_reset() {
	join();
	reset();
}

void join_and_reset(void(*task)(), clock::duration interval) {
	clock::time_point prev = clock::now() - interval;
	while (n_tasks && !quit.load(::std::memory_order_acquire)) {
		const clock::time_point now { clock::now()       };
		const clock::time_point next{ process_timed(now) };

		while (now > (prev + interval)) {
			task();
			prev += interval;
		}

		process_io_and_wait(::std::min({ now + 100us, prev + interval, next }));
	}
	reset();
}

void kill() noexcept {
	quit = true;
}

void shut_down() {
	quit = true;
	cv.notify_all();
	for (auto& t : threads)
		t.join();
	#ifdef __linux
	::io_uring_queue_exit(&ring);
	#else
	#endif
}

::size_t size() noexcept {
	return threads.size();
}

handle handle::resume() && {
	if (has_fiber()) {
		fiber* const ptr = get_fiber();
		ptr->f = ::std::move(ptr->f).resume();
		if (ptr->msg) [[unlikely]] {
			n_tasks++;	
			return ::std::move(*this).process_msg();
		} else {
			return ::std::move(*this);
		}
	} else {
		(*static_cast<jpl::function<void()>*>(ptr))();
		ptr = nullptr;
		return ::std::move(*this);
	}
}

handle handle::process_msg() && {
	fiber* const ptr = get_fiber();
	void* msg{ ptr->msg };
	ptr->msg = nullptr;
	switch (*static_cast<msg_type*>(msg)) {
		case msg_type::read_file: {
			read_file_msg& read_file = *static_cast<read_file_msg*>(msg);
			#ifdef __linux
				read_file.task = ptr;
				{
					::std::lock_guard lock{ uring_mtx };
					read_file.next = read_queue;
					read_queue = static_cast<read_file_msg*>(msg);
				}
			#else
				read_file.task = ptr;
				io_events.queue(static_cast<read_file_msg*>(msg));
			#endif
			handle::ptr = nullptr;
		} break;
		case msg_type::wait_until: {
			wait_until_msg& wait_until = *static_cast<wait_until_msg*>(msg);
			{
				::std::lock_guard lock{ delayed_mtx };
				timed_tasks.emplace(timed_task{ ptr, wait_until.when });
				next_timed_task = ::std::min(wait_until.when, next_timed_task);
			}
			::delete static_cast<wait_until_msg*>(msg);
			handle::ptr = nullptr;
		} break;
		case msg_type::exception: {
			quit = true;
			cv.notify_all();
		} break;
	}
	return ::std::move(*this);
}

bool fiber::try_yield() /* can throw (yield) */ {
	bool task_available = []{
		::std::lock_guard lock{ tp::mtx };
		return !tp::tasks.empty();
	}();
	if (task_available) {
		yield();
		return true;
	} else {
		return false;
	}
}

void fiber::handle_exception() noexcept {
	msg = (void*)&::jpl::tp::exception_msg;
	::std::lock_guard lock{ ::jpl::tp::mtx };
	::jpl::tp::exceptions.emplace_back(::std::current_exception());
}

#ifdef __linux
	thread_local ::jpl::vector<::uint8_t> vec;
	::jpl::file_data fiber::read_file(const char *path) {
		int fd = ::open(path, O_RDONLY);
		if (fd == -1) [[unlikely]]
			return ::jpl::file_data::err::not_found;
		struct ::stat sb;
		[[maybe_unused]] int err = ::fstat(fd, &sb);
		assert(err == 0);
		void* map = ::mmap(nullptr, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);
		if (map == nullptr) [[unlikely]]
			return ::jpl::file_data::err::map_failed;
		vec.resize( (sb.st_size + tp::page_size - 1) / tp::page_size );
		err = ::mincore(map, sb.st_size, vec.data());
		assert(err == 0);
		::size_t cached = 0;
		for (auto c : vec)
			cached += (c & 1);
		if (cached == vec.size()) {
			::close(fd);
			return { static_cast<char*>(map), static_cast<::uint32_t>(sb.st_size), ::jpl::file_data::type::mmap };
		} else {
			::munmap(map, sb.st_size);
			// Only a limited number of file handles can be open, so close here and reopen when processed to avoid overflow
			::close(fd);
			// memory doesn't need to be aligned, but empirically page size alignment leads to better performance
			::jpl::file_data data{ ::aligned_alloc(tp::page_size, sb.st_size), static_cast<::uint32_t>(sb.st_size), ::jpl::file_data::type::alloc };
			msg = ::new tp::read_file_msg{ tp::msg_type::read_file, path, &data };
			yield();
			return data;
		}
	}
#else // Windows
	static bool check_error() {
		auto err = ::GetLastError();
		if (err == ERROR_IO_PENDING) [[likely]]
			return true;
		LPWSTR wstr = nullptr;
		auto wlen = ::FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, nullptr, err, 0, (LPWSTR)&wstr, 0, nullptr);
		auto len = ::WideCharToMultiByte(CP_UTF8, 0, wstr, wlen, nullptr, 0, 0, false);
		::std::string str( len, '\0' );
		::WideCharToMultiByte(CP_UTF8, 0, wstr, wlen, str.data(), str.size(), 0, false);
		throw ::std::runtime_error(str);
	}
	::jpl::vector<::std::byte> fiber::read_file(const char *path) {
			wchar_t wpath[1024]{};
			::MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, path, strlen(path), wpath, 1024);
			HANDLE fd = ::CreateFileW(wpath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_FLAG_OVERLAPPED, nullptr);
			assert(fd != INVALID_HANDLE_VALUE);
			LARGE_INTEGER sz;
			::GetFileSizeEx(fd, &sz);
			::jpl::vector<::std::byte> data{ static_cast<::size_t>(sz.QuadPart) };
			::OVERLAPPED ol{ .hEvent = ::CreateEvent(nullptr, false, false, nullptr) };
			int success = ::ReadFile(fd, data.data(), data.size(), nullptr, &ol);
			if (success || (check_error() && (::WaitForSingleObject(ol.hEvent, 0) == WAIT_OBJECT_0))) {
				// Data was immediately ready (cached), no reason to message IO thread
				::CloseHandle(ol.hEvent);
				::CloseHandle(fd);
			} else {
				msg = ::new tp::read_file_msg{ tp::msg_type::read_file, path, fd, ol.hEvent };
				yield();
			}
			return data;
	}
#endif

void fiber::wait_until(tp::clock::time_point until) {
	msg = ::new tp::wait_until_msg{ tp::msg_type::wait_until, until };
	yield();
}

void fiber::yield_or_wait(tp::clock::duration duration) {
	#ifdef __linux
	const auto until = tp::clock::now() + duration;
	#endif
	bool task_available = []{
		::std::lock_guard lock{ tp::mtx };
		return !tp::tasks.empty();
	}();
	#ifdef __linux
		if (task_available)
			msg = ::new tp::wait_until_msg{ tp::msg_type::wait_until, until };
		yield();
	#else
		if (!task_available)
			yield();
	#endif
}

} // namespace jpl::tp
