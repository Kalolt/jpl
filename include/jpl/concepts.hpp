#ifndef JPL_CONCEPTS_HPP
#define JPL_CONCEPTS_HPP

#include <type_traits>
#include <cstddef> // ::std::byte
#include <concepts>

#include <jpl/bits/invoke.hpp>

// Include a quickly compiling header that contains std::begin and std::end for ADL trick.
// On GCC <iterator> compiles slow and <span> fast, whereas on Clang it's the opposite,
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

template<class T>
concept lval_ref = ::std::is_lvalue_reference_v<T>;

using ::std::begin;
using ::std::end;
using ::std::rbegin;
using ::std::rend;
using ::std::size;

template<class T>
concept range = requires(T& t) {
	begin(t);
	end  (t);
};

template<class T>
concept sized_range = range<T> && requires(T& t) {
	size(t);
};

template<class T>
concept non_dangling_range = range<T> && requires(T& t) {
	{ *begin(t) } -> lval_ref;
};

template<class T, class ... Args>
concept range_of = range<T> && requires(T& t) {
	requires (std::same_as<std::remove_cvref_t<decltype(*begin(t))>, std::remove_cvref_t<Args>> || ...);
};

template<class T, class ... Args>
concept not_range_of = range<T> && requires(T& t) {
	requires !(std::same_as<std::remove_reference_t<decltype(*begin(t))>, Args> || ...);
};

template<class T, class U>
concept sized_range_of = sized_range<T> && requires(T& t) {
	requires std::same_as<std::remove_reference_t<decltype(*begin(t))>, U>;
};

template<class T>
concept reversible_range = requires(T& t) {
	rbegin(t);
	rend  (t);
};

template<class T, class ... Args>
concept constructible_from = requires(Args&& ... args) {
	T{ static_cast<Args&&>(args)... };
};

template<class U, class T>
concept can_construct = requires(U&& u) {
	T{ static_cast<U&&>(u) };
};

template<class T, class U>
concept convertible_range = requires(T& t) {
	{ *begin(t) } -> can_construct<U>;
	{ end(t) };
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

template<class F, class Range, class To>
concept projection = requires(Range& range, F& f) {
	{ ::jpl::invoke(f, *begin(range)) } -> convertible<To>;
};

} // namespace jpl

#endif // JPL_CONCEPTS_HPP