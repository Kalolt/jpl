#ifndef JPL_FILE_TYPES_HPP
#define JPL_FILE_TYPES_HPP

#include <cstddef>
#include <string_view>

namespace jpl {

enum class file_ext {
	unknown,
	jpeg,
	png,
	bmp,
	tga,
	gif,
	zip,
	rar,
	zip7,
};
file_ext get_ext(::std::string_view path);

} // namespace jpl

#endif // JPL_FILE_TYPES_HPP

#ifdef JPL_HEADER_ONLY
#ifndef JPL_FILE_TYPES_IMPL
#define JPL_FILE_TYPES_IMPL
#include <jpl/src/file_types.cpp>
#endif
#endif
