#include <jpl/bits/err.hpp>
#include <fmt/format.h>
#include <fmt/compile.h>
#include <stdexcept>

namespace jpl::err {

[[noreturn]] void generic(const char* what) {
	throw ::std::runtime_error{ ::fmt::format(FMT_COMPILE("{}"), what) };
}

[[noreturn]] void std(const char* what) {
	throw ::std::runtime_error{ ::fmt::format(FMT_COMPILE("{} - {}"), what, ::std::strerror(errno)) };
}

[[noreturn]] void open(const char* path) {
	throw ::std::runtime_error{ ::fmt::format(FMT_COMPILE("Unable to open {} - {}"), path, ::std::strerror(errno)) };
}

[[noreturn]] void mmap() {
	throw ::std::runtime_error{ ::fmt::format(FMT_COMPILE("mmap failed - {}"), ::std::strerror(errno)) };
}

[[noreturn]] void archive(const char* what, const char* errstr) {
	throw ::std::runtime_error{ ::fmt::format(FMT_COMPILE("{} - {}"), what, errstr) };
}

} // namespace jpl::err
