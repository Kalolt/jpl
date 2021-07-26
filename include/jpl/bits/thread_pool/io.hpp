#ifndef JPL_BITS_THREAD_POOL_IO_HPP
#define JPL_BITS_THREAD_POOL_IO_HPP

#include <chrono>

namespace jpl::tp {

using clock = ::std::chrono::steady_clock;

void init_io();
void process_io(clock::duration timeout);
void free_io();

} // namespace jpl::tp

#ifdef JPL_HEADER_ONLY
#ifndef JPL_THREAD_POOL_IO_IMPL
#define JPL_THREAD_POOL_IO_IMPL
#ifdef __linux__
#include <jpl/src/thread_pool/io_uring.cpp>
#endif
#endif
#endif

#endif // JPL_BITS_THREAD_POOL_IO_HPP
