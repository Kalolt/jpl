#ifndef JPL_DEFER_HPP
#define JPL_DEFER_HPP

#include <type_traits>

namespace jpl::detail {

template<class T>
struct defer_impl {
	T func;
	~defer_impl() { func(); }
};
template<class T>
defer_impl(T func) -> defer_impl<typename ::std::remove_reference_t<T>>;

} // namespace jpl::detail

#define JPL_DEFER_CONCAT_IMPL(A, B) A##B
#define JPL_DEFER_CONCAT(A, B) JPL_DEFER_CONCAT_IMPL(A, B)
#define jpl_defer(...)\
::jpl::detail::defer_impl JPL_DEFER_CONCAT(DEFER,__LINE__) { [&](){ __VA_ARGS__; } }

#endif // JPL_DEFER_HPP
