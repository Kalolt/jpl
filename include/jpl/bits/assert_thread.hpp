/*
	JPL_ASSERT_THREAD asserts that thread unsafe code is executed thread-safely.
	To ensure that a function/scope is only called from a single thread at a time,
	simply call JPL_ASSERT_THREAD_SIMPLE()

	void thread_unsafe_function() {
		JPL_ASSERT_THREAD_SIMPLE();
		// Do stuff
	}

	If there are multiple scopes, which are mutually thread unsafe, use JPL_ASSERT_ATOM(NAME) to generate a variable,
	and use it with JPL_ASSERT_THREAD(ATOM) as if it was a mutex.

	struct thread_unsafe_struct {
		JPL_ASSERT_ATOM(assert_flag);

		void method1() {
			JPL_ASSERT_THREAD(assert_flag);
			// Do stuff
		}

		void method2() {
			JPL_ASSERT_THREAD(assert_flag);
			// Do stuff
		}
	};
*/

#ifndef JPL_ASSERT_HPP
#define JPL_ASSERT_HPP

#if defined(NDEBUG) || defined(JPL_NDEBUG_THREAD)
#define JPL_ASSERT_ATOM(...)
#define JPL_ASSERT_THREAD(...)
#else
#include <atomic>
#include <stdio.h>

#include <jpl/defer.hpp>
#include <jpl/bits/get_file_func.hpp>

#define JPL_ASSERT_THREAD_SIMPLE()\
	static ::std::atomic<bool> JPL_ASSERT_SAFE = true;\
	bool JPL_ASSERT_EXPECTED = true;\
	JPL_ASSERT_SAFE.compare_exchange_strong(JPL_ASSERT_EXPECTED, false, ::std::memory_order_release, ::std::memory_order_relaxed);\
	if (!JPL_ASSERT_EXPECTED) [[unlikely]] {\
		JPL_GET_FILE_FUNC(location)\
		::fprintf(stderr, "Thread safety assertion failed at: %s\n", location);\
	}\
	jpl_defer( JPL_ASSERT_SAFE = true; );
#define JPL_ASSERT_ATOM(NAME)\
	::std::atomic<const char*> NAME{ nullptr }
#define JPL_ASSERT_THREAD(ATOM)\
	const char* JPL_ASSERT_EXPECTED;\
	JPL_GET_FILE_FUNC(location)\
	ATOM.compare_exchange_strong(JPL_ASSERT_EXPECTED, location, ::std::memory_order_release, ::std::memory_order_relaxed);\
	if (JPL_ASSERT_EXPECTED) [[unlikely]] {\
		::fprintf(stderr, "Thread safety assertion failed at: %s\nAnother thread is running:         %s\n", location, JPL_ASSERT_EXPECTED);\
	}\
	jpl_defer( ATOM = nullptr; )
#endif

#endif // JPL_ASSERT_HPP
