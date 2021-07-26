# Miscellaneous C++20 libraries.

The main supported platform for these libraries is Linux x86-64, but almost everything should work on Windows x86-64 as well.

Additionally, since the library is currently mainly for personal use, some parts (such as random.hpp) use intrinsics for instructions such as RDTSC and RDSEED.

The library uses cutting-edge C++20 features as soon as all 3 major compilers support them, so up-to-date compiler is required:
+ GCC - 10
+ Clang - 12
+ MSVC - latest version

API stability is not guaranteed for now.

Documentation (incomplete):
+ [jpl::vector](https://github.com/JPylkkanen/jpl/blob/master/documentation/vector.md)
+ [jpl::function](https://github.com/JPylkkanen/jpl/blob/master/documentation/function.md)
+ [jpl_defer](https://github.com/JPylkkanen/jpl/blob/master/documentation/defer.md)

Third party dependencies:
+ archive.hpp - [libarchive](https://github.com/libarchive/libarchive), [libdeflate](https://github.com/ebiggers/libdeflate), [frozen](https://github.com/serge-sans-paille/frozen)
+ thread_pool.hpp - [Boost.Context](https://github.com/boostorg/context), [liburing](https://github.com/axboe/liburing)
+ Unit tests - [doctest](https://github.com/onqtam/doctest)
