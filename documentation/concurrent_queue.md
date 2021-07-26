## jpl_defer<class T, uint32_t ring_buffer_size = 1024, bool use_optional = true> requires((std::popcount(ring_buffer_size) == 1) && std::is_nothrow_move_constructible_v<T> && std::is_nothrow_destructible_v<T>)

Multi-producer multi-consumer concurrent queue.

# Features:

- All methods are thread-safe. No need for external synchronization.
- The queue uses a fixed size ring buffer, and thus never does any dynamic allocations.
- Works with any type as long it is nothrow destructible and nothrow movable. Optionally to push values by copy, the type also needs to be nothrow copyable.
- High performance
	- The core synchronization mechanism is lock-free, and for blocking methods also wait-free, although the data structure as a whole is neither, because methods can wait on per-element futexes. However, apart from extreme edge cases, any single futex should have at most 1 waiter at a time, so the use of futexes shouldn't hurt the scalability of the queue.
	- Data layout and ring buffer indexing order have been optimized to avoid false sharing, with minimal reduction in true sharing.

# Downsides:

- No NUMA awareness, and it might be hard, if not impossible, to make this design NUMA aware, although I haven't really looked into it.
- The fixed size buffer combined with blocking operations can lead into a deadlock. If the queue is full, and there are threads trying to push() elements into it with no thread trying to pop(), the threads trying to push() will be indefinitely blocked.
	- For example, if you use the queue as a task queue for a thread pool, and you had a system where any task can push more tasks to the queue, you could end up in a situation, where every thread on the thread pool is blocked trying to add tasks to a full task queue.

# Template parameteres:

template<class T, ::uint32_t ring_buffer_size, bool use_optional = true>
- T - type contained in the queue
- ring_buffer_size - size of the internal ring buffer (must be a power of 2)
	- When possible, ring_buffer_size should be set big enough, that it's never full, so that push() never has to block.
- use_optional - This configures whether or not to return std::optional<T> or plain T from try_pop()
	- Some types have a natural null state. In such case wrapping the type in std::optional is pointless, and it makes more sense to return a default constructed value to denote failure in try_pop().

# Macro config:

You can define JPL_CACHE_LINE_SIZE macro to configure the assumed cache line size, which is used in avoiding false sharing.
The default value is set to 64 bytes, which should be the correct value on all modern x86-64 platforms.

# Methods:

- T pop();
Pops a value from the queue. If the queue is empty, blocks until a value becomes available.

- optional_t try_pop();
using optional_t = std::conditional\<use_optional, std::optional\<T\>, T\>;
Pops a value from the queue. If the queue is empty, returns default constructed optional_t.
Note: due to the unpredictable nature of multi-producer multi-consumer queues, another thread could add value to the queue simultaneously, and this could still return std::nullopt, so there's some inherent raciness.
However, this will never return a false negative, meaning that if the queue wasn't empty at any point during the execution of this method, this will pop a value.

- bool try_pop(T& out);
Alternative way to call try_pop. This version will try to move-assign the value into the input parameter, and returns bool indicating success or failure.
This version can perform slightly better than other one.

- void push(T&& val) noexcept;
Moves a value to the queue. If the internal ring buffer is full, blocks until another thread pops values from the queue.

- void push(const T& val) noexcept requires(::std::is_nothrow_copy_constructible_v<T>);
Copies a value to the queue. If the internal ring buffer is full, blocks until another thread pops values from the queue.

- bool try_push(T&& val) noexcept;
Tries to move a value to the queue. Returns true, if successful. If the internal ring buffer is full, returns false.

- bool try_push(const T& val) noexcept requires(::std::is_nothrow_copy_constructible_v<T>);
Tries to copy a value to the queue. Returns true, if successful. If the internal ring buffer is full, returns false.

- There are no methods such as empty() or size(), because those don't have any meaning in a multi-producer multi-consumer scenario, where the result could be obsolete before you even have a chance to check it.

# Usage tips:

- Try to make accesses as quick as possible by using a type that can be quickly moved. For more complex types it's wise to use std::unique_ptr.
- If you want to drain the queue empty after joining all other threads, just call try_pop until it returns std::nullopt.

# How does it work?

The core principle behind the queue is that it has a ring buffer with atomic head and tail, which are only ever incremented (and which handle overflow seamlessly). The push/pop methods get a turn number by atomically incrementing the tail or head respectively. The turn number is then converted into an index within the ring buffer.

This is very scalable, because the tail and the head atomics are the only thread synchronization primitives used simultaneously by all threads, and the increment can usually be done in a wait-free manner with std::atomic::fetch_add().
Only in the case of try_push() and try_pop() is the increment done with compare-and-swap (CAS), which can involve waiting.

The ring buffer is an array of nodes, which contain the stored value, and an atomic integer denoting which turn number gets to access it next, so when a push/pop method has received a turn number, it checks the status of the node to see if it's its turn. If the node isn't ready, the push/pop method will wait on a futex for the value to change. When the value changes, the method reads/writes data, and then updates the node status to denote that it's ready for the next read/write.

Accesses to the ring buffer are interleaved to avoid false sharing. Any consequent 4 nodes are guaranteed to be on different cache lines, but then it reuses nodes from the already cached area. For example, if there are 16 nodes per cache line, the access pattern will be: [0, 16, 32, 48, 1, 17, 33, 49, 2, 18, 34, 50, etc.].

It should be noted that since the turn numbers are reused after overflow, in theory it's possible to break the queue, if you have more than UINT32_MAX threads trying to simultaneously push or pop values. However, good luck trying to create so many threads.

# Notes about performance and benchmarking

I've found meaningfully benchmarking the queue quite difficult for various reasons. It's easy to make benchmarks that might not be indicative of real world performance.

For example, the queue does quite poorly in benchmarks, where the consumers outpace the producers, and the objects put on the queue are trivial. This is because when the queue is constantly instantly emptied, it leads to a scenario, where every pop() has to wait on a futex, and every push() has to notify the futex, so both sides make a syscall on every action.
When the benchmark does nothing put push() and pop() trivial values on the queue, the futex syscalls end up taking most of the time.
In such benchmarks replacing pop() with a try_pop() loop leads to much better performance, because try_pop() never waits on a futex, and push() doesn't do FUTEX_WAKE when there are no waiters.

However, I believe that this is practically completely a synthetic benchmark-specific issue to begin with. In any sane program you won't just have a loop that pushes a million meaningless values to the queue on a loop that does nothing else.
Replacing the futex by simply busy-waiting gives orders of magnitude better results in trivial benchmarks, but realistically in most practical situations, I believe that there are 2 common usage patterns for the queue:
1. The consumers work faster than producers, so the queue is empty most of the time.
In any non-trivial case this will not be because the producers are bottle-necked by FUTEX_WAKE, but because they simply can't produce values fast enough. In this case it's perfectly reasonable for idle consumers to wait on a futex, because it's the best combination of saving system resources and being able to respond fast to when there eventually is something to consume.
Busy-waiting would react to changes faster, but waste lots of CPU cycles.
Polling with occasional sleeping/yielding would be gentler than busy-waiting on system resources and remove the need for push() to do FUTEX_WAKE, but could lead to worse response times than futex.
2. The queue is mostly half-filled with consumers almost always immediately getting something to process.
In this scenario jpl::concurrent_queue is at its best, because when the queue is not empty, the producers and consumers don't need to wait for each other, and both sides can do their thing in fully lock-free and wait-free manner. This is where jpl::concurrent_queue will outperform almost any other MCMP queue by a mile.

For example, if the queue is used as a task queue for a thread pool in a 3D game engine with VSync, it will be in state 2 when a new frame starts getting drawn, and state 1 as it waits for VSync.

There's also the issue of setting appropriate ring_buffer_size when benchmarking. In normal usage you should configure ring_buffer_size so that it should never be full. However, in benchmarks that push() and pop() values as fast as they can, the required buffer size can get ridiculously large, so in a benchmark you might reasonably need to use ring_buffer_size = 65536 or even larger to get proper results.

It kinda feels like cheating to get massive performance boost in benchmarks by using an unrealistically huge ring_buffer_size, but actually it should be more representative of what the performance is like when the ring_buffer_size is properly configured, because in practice the push() method should never need to wait for values to be popped from the queue, and if you benchmark with the a small ring_buffer_size, the benchmark will spend half of the runtime just waiting in push().

# Example 1: program that creates a bunch of producer and consumer threads.
If consumers received correct data from the queue, it should print 0 as a result.

```C++
#include <cstdio>
#include <thread>
#include <jpl/concurrent_queue.hpp>
#include <jpl/vector.hpp>

int main() {
	constexpr int n_threads = 16;
	constexpr int per_thread = 500'000;

	jpl::concurrent_queue<int> rb;
	std::atomic<int> xor_shared{ 0 };

	jpl::vector<std::thread, n_threads> producers;
	jpl::vector<std::thread, n_threads> consumers;

	for (int thread_id = 0; thread_id != n_threads; ++thread_id) {
		producers.emplace_back([&, thread_id]{
			for (int i = 0; i < per_thread; i++)
				rb.push(thread_id * per_thread + i);
		});
		consumers.emplace_back([&, thread_id]{
			int xor_local = 0;
			for (int i = 0; i < per_thread; i++) {
				int val = rb.pop();
				xor_local ^= val ^ (thread_id * per_thread + i);
			}
			xor_shared.fetch_xor(xor_local);
		});
	}

	for (auto& t : producers) t.join();
	for (auto& t : consumers) t.join();

	std::printf("%d\n", xor_shared.load());
}
```

# Example 2: same as above, but using try_pop() instead of pop().
Note: try_pop() performs worse than pop() in an example like this. try_pop() should only be used when the thread has something else it can do, if the queue is empty.

```C++
#include <cstdio>
#include <jpl/concurrent_queue.hpp>
#include <jpl/vector.hpp>
#include <thread>
#include <mutex>

int main() {
	constexpr int n_threads = 16;
	constexpr int per_thread = 500'000;

	jpl::concurrent_queue<int> rb;
	int xor_test{ 0 };
	std::mutex m;

	jpl::vector<std::thread, n_threads> producers;
	jpl::vector<std::thread, n_threads> consumers;

	for (int thread_id = 0; thread_id != n_threads; ++thread_id) {
		producers.emplace_back([&, thread_id]{
			for (int i = 0; i < per_thread; i++)
				rb.push(thread_id * per_thread + i);
		});
		consumers.emplace_back([&, thread_id]{
			int xor_local = 0;
			for (int i = 0; i < per_thread; i++) {
				std::optional<int> val = rb.try_pop();
				while (!val) {
					std::this_thread::yield(); // Here you should do something interesting instead
					val = rb.try_pop();
				}
				xor_local ^= *val ^ (thread_id * per_thread + i);
			}
			std::scoped_lock lock{ m };
			xor_test ^= xor_local;
		});
	}

	for (auto& t : producers) t.join();
	for (auto& t : consumers) t.join();

	std::printf("%d\n", xor_test);
}
```
