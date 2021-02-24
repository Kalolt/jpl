#ifndef JPL_CONCEPTS_HPP
#define JPL_CONCEPTS_HPP

#include <type_traits>
#include <cstddef> // ::std::byte
#include <concepts>

// Include a quickly compiling header that contains std::begin and std::end for ADL trick.
// On GCC <iterator> compiles slow and all containers fast, whereas on Clang it's the opposite,
// so select based on compiler.
#ifdef __llvm__
#include <iterator>
#else
#include <span>
#endif

namespace jpl {

template<class T>
concept trivial = ::std::is_trivial_v<T>;

template<class F, class... Args>
concept invocable = ::std::is_invocable_v<F, Args...>;

template<class R, class F, class... Args>
concept invocable_r = ::std::is_invocable_r_v<R, F, Args...>;

template<class T, class U>
concept can_add = requires(T t, U u) { t + u; };

template<class T, class U>
concept can_reduce = requires(T t, U u) { t - u; };

template<class From, class To>
concept convertible = ::std::is_convertible_v<From, To>;

using ::std::begin;
using ::std::end;
using ::std::rbegin;
using ::std::rend;

template<class T>
concept range = requires(T& t) {
	begin(t);
	end  (t);
};

template<class T>
concept reversible_range = requires(T& t) {
	rbegin(t);
	rend  (t);
};

template<class T, class U>
concept convertible_range = requires(T& t) {
	U{ *begin(t) };
	U{ *end  (t) };
};

template<class T, class U>
concept constructible = requires(U&& u) {
	T{ static_cast<U&&>(u) };
};

template<class T>
concept byte_or_void = ::std::same_as<::std::remove_cv_t<T>, ::std::byte>
                    || ::std::same_as<::std::remove_cv_t<T>, char>
                    || ::std::same_as<::std::remove_cv_t<T>, unsigned char>
                    || ::std::same_as<::std::remove_cv_t<T>, void>;

template<class T, class U>
concept can_alias = ::std::same_as<T, U> || (byte_or_void<T> && byte_or_void<U>);

template<class T>
concept container = requires(T& t) {
	t.data();
	t.size();
};

template<class T>
concept integral = ::std::is_integral_v<T>;

} // namespace jpl

#endif // JPL_CONCEPTS_HPP