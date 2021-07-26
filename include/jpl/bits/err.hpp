#ifndef JPL_ERR_HPP
#define JPL_ERR_HPP

#ifdef JPL_HEADER_ONLY
#define JPL_INLINE inline
#else
#define JPL_INLINE
#endif

namespace jpl::err {

[[noreturn]] JPL_INLINE void generic(const char* path);
[[noreturn]] JPL_INLINE void std(const char* path);
[[noreturn]] JPL_INLINE void open(const char* path);
[[noreturn]] JPL_INLINE void mmap();
[[noreturn]] JPL_INLINE void archive(const char* what, const char* errstr);

} // namespace jpl::err

#ifdef JPL_HEADER_ONLY
#include <jpl/src/bits/err.cpp>
#endif

#endif // JPL_ERR_HPP
