#ifndef JPL_TRIVIALLY_RELOCATABLE_HPP
#define JPL_TRIVIALLY_RELOCATABLE_HPP

#include <type_traits>

namespace jpl {

template<class T>
inline constexpr bool trivially_relocatable = ::std::is_trivial_v<T>;

} // namespace jpl

#endif // JPL_TRIVIALLY_RELOCATABLE_HPP
