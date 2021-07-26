#include <chrono>
#include <jpl/thread_pool.hpp>
#include <jpl/vector.hpp>
#include <jpl/bits/thread_pool/io.hpp>

#include <stdexcept>
#include <thread>
#include <mutex>

#ifdef __linux__
#include <linux/io_uring.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <time.h>
#else
#define NOMINMAX
#define WIN32_MEAN_AND_LEAN
#include <windows.h>
#endif

#include <fmt/format.h>
#include <immintrin.h>

namespace jpl::tp {

using namespace std::chrono_literals;

void* uring_ptr;
::io_uring_sqe* sqes;
::io_uring_sqe timeout_sqe;
::timespec timeout_ts{};
::io_uring_cqe* cqes;
int fd;
unsigned* sq_head;
unsigned* sq_tail;
unsigned* cq_head;
unsigned* cq_tail;
unsigned  sq_mask;
unsigned  cq_mask;
::std::atomic<unsigned> sq_tail_local;
::std::atomic<unsigned> sq_head_local; // This is needed to avoid a race condition!
struct request_t {
	unsigned turn;
	bool active;
};
::std::atomic<request_t> turn_request{ { 0, false } };
::std::unique_ptr<::std::atomic<unsigned>[]> sqe_sync;
::uint32_t sqes_size;
::uint32_t uring_size;
unsigned sq_entries;
unsigned pending_io = 0;

read_file_awaiter::read_file_awaiter(int fd) : fd{ fd } {
	struct ::stat st;
	::fstat(fd, &st); // TODO: check return value
	buffer.resize(st.st_size);
}

struct empty_promise {
	struct promise_type {
		static constexpr empty_promise get_return_object() noexcept { return {}; }
		static constexpr std::suspend_never initial_suspend() noexcept { return {}; }
		static constexpr std::suspend_never final_suspend() noexcept { return {}; }
		static void unhandled_exception() { throw; }
		static constexpr void return_void() noexcept {}
	};
};

void fill_sqe(::std::coroutine_handle<> handle, unsigned turn, jpl::vector<char>& buffer) {
	unsigned idx = turn & sq_mask;
	::io_uring_sqe& sqe{ sqes[idx] };
	sqe.opcode = IORING_OP_READ;
	sqe.flags = 0;
	sqe.ioprio = 0;
	sqe.fd = fd;
	sqe.off = 0;
	sqe.addr = reinterpret_cast<::uint64_t>(buffer.data());
	sqe.len = buffer.size();
	sqe.rw_flags = 0;
	::memcpy(&sqe.user_data, &handle, sizeof(handle));
	sqe.__pad2[0] = sqe.__pad2[1] = sqe.__pad2[2] = 0;

	detail::pending_tasks++;
	sqe_sync[idx] = turn + 1;
}

inline empty_promise get_turn_wait(::std::coroutine_handle<> handle, unsigned turn, jpl::vector<char>& buffer) noexcept {
	while ((turn - sq_head_local) >= (sq_entries - 1)) [[unlikely]] {
		request_t request = turn_request;
		if (request.active && (turn == request.turn)) {
			turn = sq_tail_local++;
			turn_request = request_t{ 0, false };
		}
		if ((turn - sq_head_local) > sq_entries)
			co_await jpl::tp::sleep_for{ 5ms };
	}
	fill_sqe(handle, turn, buffer);
}

void read_file_awaiter::await_suspend(::std::coroutine_handle<> handle) {
	unsigned turn = sq_tail_local.fetch_add(1, ::std::memory_order::acquire);
	if ((turn - sq_head_local) >= (sq_entries - 1)) [[unlikely]]
		get_turn_wait(handle, turn, buffer);
	else
		fill_sqe(handle, turn, buffer);
}

read_file_awaiter read_file(const char* file_path) {
	int file_fd = ::open(file_path, O_RDONLY);
	if (file_fd < 0)
		throw ::std::runtime_error("Unable to open file");
	return file_fd;
}

void init_io() {
	const char* err;
	::io_uring_params params{};
	unsigned* sq_array; // This needs to be forward declared for gotos to work

	fd = ::syscall(SYS_io_uring_setup, 512, &params);
	if (fd < 0) {
		err = "io_uring_setup failed";
		goto err1;
	}

	uring_size = ::std::max(
		params.sq_off.array + params.sq_entries * sizeof(unsigned),
		params.cq_off.cqes  + params.cq_entries * sizeof(::io_uring_cqe)
	);

	sqe_sync = ::std::make_unique<::std::atomic<unsigned>[]>(params.sq_entries);
	for (unsigned i = 0; i != params.sq_entries; ++i)
		sqe_sync[i] = i;

	if (!(params.features & IORING_FEAT_SINGLE_MMAP)) {
		err = "jpl::tp requires a kernel version that supports IORING_FEAT_SINGLE_MMAP";
		goto err2;
	}

	uring_ptr = ::mmap(0, uring_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, fd, IORING_OFF_SQ_RING);
	if (uring_ptr == MAP_FAILED) {
		err = "io_uring mmap failed";
		goto err2;
	}

	// These offsets might be in bytes, in which case they should be applied to char*
	sq_head =  reinterpret_cast<unsigned*>(static_cast<char*>(uring_ptr) + params.sq_off.head);
	sq_tail =  reinterpret_cast<unsigned*>(static_cast<char*>(uring_ptr) + params.sq_off.tail);
	sq_mask = *reinterpret_cast<unsigned*>(static_cast<char*>(uring_ptr) + params.sq_off.ring_mask);
	cq_head =  reinterpret_cast<unsigned*>(static_cast<char*>(uring_ptr) + params.cq_off.head);
	cq_tail =  reinterpret_cast<unsigned*>(static_cast<char*>(uring_ptr) + params.cq_off.tail);
	cq_mask = *reinterpret_cast<unsigned*>(static_cast<char*>(uring_ptr) + params.cq_off.ring_mask);
	
	sq_entries = params.sq_entries;

	sq_array = reinterpret_cast<unsigned*>(static_cast<char*>(uring_ptr) + params.sq_off.array);
	for (unsigned i = 0; i != params.sq_entries; ++i)
		sq_array[i] = i;

	sq_head_local = *sq_head;
	sq_tail_local = *sq_tail;

	sqes_size = params.sq_entries * sizeof(::io_uring_sqe);

	sqes = static_cast<::io_uring_sqe*>(::mmap(0, sqes_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, fd, IORING_OFF_SQES));
	if (sqes == MAP_FAILED) {
		err = "io_uring mmap failed";
		goto err3;
	}
	cqes = reinterpret_cast<::io_uring_cqe*>(static_cast<char*>(uring_ptr) + params.cq_off.cqes);

	timeout_sqe = io_uring_sqe{
		.opcode = IORING_OP_TIMEOUT,
		.fd = fd,
		.off = 1, // Number of events to wait for!
		.addr = reinterpret_cast<::uint64_t>(&timeout_ts),
		.len = 1,
	};
	return;

	err3: ::munmap(uring_ptr, uring_size);
	err2: ::close(fd);
	err1: throw ::std::runtime_error{ err };
}

void free_io() {
	::munmap(sqes     , sqes_size);
	::munmap(uring_ptr, uring_size);
	::close(fd);
}

void add_timeout_event(clock::duration timeout) {
	unsigned turn = sq_tail_local;
	bool success = false;
	do {
		sq_head_local = *sq_head;
		if ((sq_tail_local - sq_head_local) >= (sq_entries)) [[unlikely]] {
			request_t request = turn_request;
			if (request.active)
				continue;
			turn = sq_head_local + sq_entries - 1;
			turn_request = request_t{ turn, true };
			success = true;
		} else {
			success = sq_tail_local.compare_exchange_strong(turn, turn + 1);
		}
	} while (!success);
	unsigned idx = turn & sq_mask;

	::memcpy(&sqes[idx], &timeout_sqe, sizeof(::io_uring_sqe));
	timeout_ts.tv_nsec = timeout.count();
	sqe_sync[idx] = turn + 1;
}

void process_io(clock::duration timeout) {
	clock::time_point now = clock::now();
	clock::time_point deadline = now + timeout;
	unsigned to_submit = sq_tail_local - *sq_tail;

	while (to_submit || pending_io) {
		add_timeout_event(deadline - now);
		to_submit = ::std::min(sq_tail_local - *sq_tail, sq_entries);

		// Wait for current events to be ready (this should practically never actually involve waiting)
		// This is needed if an event was added by another thread immediately before process_io was called,
		// and that thread was then pre-empted between when it incremented sq_tail_atomic and when it was able to fill the SQE
		const unsigned tail = *sq_tail;
		for (unsigned i = 0; i != to_submit; ++i) {
			unsigned turn = (tail + i);
			unsigned local_idx = turn & sq_mask;
			while (sqe_sync[local_idx] != (turn + 1)) {
				process_timed();
				::std::this_thread::yield();
			}
		}

		*sq_tail += to_submit;
		pending_io += to_submit;

		asm volatile ("":::"memory");
		int res = ::syscall(SYS_io_uring_enter, fd, to_submit, 1, IORING_ENTER_GETEVENTS, nullptr);
		sq_head_local = *sq_head;
		assert(res);

		// Process CQ
		bool timed_out = false;
		unsigned head = *cq_head;
		while (head != *cq_tail) {
			unsigned idx = head & cq_mask;
			::io_uring_cqe& cqe = cqes[idx];

			if (cqe.user_data) {
				::std::coroutine_handle<> handle;
				::memcpy(&handle, &cqe.user_data, 8);
				enqueue(handle);
				detail::pending_tasks--;
			} else {
				if (cqe.res == -ETIME)
					timed_out = true;
			}

			head++;
			pending_io--;
		}
		asm volatile ("":::"memory");
		*cq_head = head;

		now = clock::now();
		if (timed_out || (now >= deadline))
			return;

		to_submit = sq_tail_local - *sq_tail;
	}

	::std::this_thread::sleep_until(deadline);
}

} // namespace jpl::tp
