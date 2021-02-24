#ifndef JPL_MISC_HPP
#define JPL_MISC_HPP

#include <jpl/vector.hpp>

#include <type_traits>
#include <array>
#include <string>

namespace jpl {

// C++20 guarantees that integers are two's complement
[[gnu::always_inline]] inline constexpr bool odd (integral auto i) noexcept { return   i & 1 ; }
[[gnu::always_inline]] inline constexpr bool even(integral auto i) noexcept { return !(i & 1); }

template<class T> requires(range<T>)
constexpr auto size_bytes(const T& range) {
	return sizeof(typename T::value_type) * range.size();
}

inline constexpr ::size_t align_size(::size_t size, ::size_t alignment) noexcept {
	return size + alignment - ((size - 1) % alignment) - 1;
}

// Optimized version that only works when alignment is a power of 2
inline constexpr ::size_t align_size_pow2(::size_t size, ::size_t alignment) noexcept {
	assert(::std::popcount(alignment) == 1);
	return size + alignment - ((size - 1) & (alignment - 1)) - 1;
}

template<class T = void, class...Types>
inline constexpr auto make_array(Types&&... t) {
	if constexpr (::std::is_void_v<T>)
		return ::std::array<::std::common_type_t<::std::decay_t<Types>...>, sizeof...(Types)>{{ ::std::forward<Types>(t)... }};
	else
		return ::std::array<T, sizeof...(Types)>{ ::std::forward<Types>(t)... };
}

// Enumerate range
template<range T>
struct enumerate {
	T iterable;

	static_assert(!std::is_const_v<decltype(iterable)>);

	using iter_t = decltype(std::begin(std::declval<T>()));
	using end_t  = decltype(std::end  (std::declval<T>()));

	struct result_type {
		::size_t i;
		decltype(*std::declval<iter_t>())& iter;
	};
	struct beg_t {
		::size_t i;
		iter_t iter;
		bool operator!=(const end_t& end) const { return iter != end; }
		void operator++() { ++i; ++iter; }
		result_type operator*() { return result_type{ i, *iter }; }
	};

	beg_t begin()       { using std::begin; return { 0, begin(iterable) }; }
	beg_t begin() const { using std::begin; return { 0, begin(iterable) }; }
	end_t end()         { using std::end  ; return end(iterable); }
	end_t end()   const { using std::end  ; return end(iterable); }
};
// Hold lvalue by reference, rvalues by value
template<range T>
enumerate(T&&) -> enumerate<::std::conditional_t<::std::is_rvalue_reference_v<T&&>, ::std::remove_reference_t<T>, ::std::remove_reference_t<T>&>>;

// Reverse range
template<reversible_range T>
struct reverse {
	T iterable;
	auto begin()       { using ::std::rbegin; return rbegin(iterable); }
	auto begin() const { using ::std::rbegin; return rbegin(iterable); }
	auto end()         { using ::std::rend  ; return rend  (iterable); }
	auto end()   const { using ::std::rend  ; return rend  (iterable); }
};
// Hold lvalue by reference, rvalues by value
template<reversible_range T>
reverse(T&&) -> reverse<::std::conditional_t<::std::is_rvalue_reference_v<T&&>, ::std::remove_reference_t<T>, ::std::remove_reference_t<T>&>>;

} // namespace jpl

#endif // JPL_MISC_HPP
