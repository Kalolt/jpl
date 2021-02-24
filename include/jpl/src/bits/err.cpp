#include <jpl/bits/err.hpp>
#include <fmt/format.h>
#include <stdexcept>

namespace jpl::err {

[[noreturn]] void open(const char* path) {
	throw ::std::runtime_error(::fmt::format(FMT_STRING("Unable to open {} - {}"), path, ::std::strerror(errno)));
}

[[noreturn]] void std(const char* what) {
	throw ::std::runtime_error(::fmt::format(FMT_STRING("{} - {}"), what, ::std::strerror(errno)));
}

[[noreturn]] void mmap() {
	throw ::std::runtime_error(::fmt::format(FMT_STRING("mmap failed - {}"), ::std::strerror(errno)));
}

[[noreturn]] void archive(const char* what, const char* errstr) {
	throw ::std::runtime_error(::fmt::format(FMT_STRING("{} - {}"), what, errstr));
}

} // namespace jpl::err
