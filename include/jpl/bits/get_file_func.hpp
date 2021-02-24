// Adds JPL_GET_FILE_FUNC(VAR_NAME) macro,
// which stores "[file name] : [line] - [function]" in a compile-time C-string

#ifndef JPL_GET_FILE_FUNC_HPP
#define JPL_GET_FILE_FUNC_HPP

#ifdef _MSC_VER

// MSVC handles __FILE__ et al. as literals rather than variables, making things very simple.

#define JPL_M_TOSTRING_IMPL(x) #x
#define JPL_M_TOSTRING(x) JPL_M_TOSTRING_IMPL(x)
#define JPL_GET_FILE_FUNC(VAR_NAME)\
static constexpr const char* VAR_NAME = __FILE__ " : " JPL_M_TOSTRING(__LINE__) " - " __FUNCTION__;

#else // !_MSC_VER

#include <utility>

namespace jpl::detail {

inline consteval int n_digits(int n) {
	int res = 1;
	while (n /= 10)
		res++;
	return res;
}

template<::size_t n>
inline consteval ::size_t strlen(const char (&)[n]) {
	return n - 1;
}

template<int len>
inline consteval char get_digit_char(int n, [[maybe_unused]] int i) {
	i = len - 1 - i;
	int div = 1;
	while (i) {
		div *= 10;
		i--;
	}
	return '0' + ((n / div) % (10));
}

template<::size_t len_func, ::size_t len_file, int line>
struct concat {
	const char str[7 + len_func + len_file + n_digits(line)];

	template<::size_t ... func_idx_seq, ::size_t ... file_idx_seq, ::size_t ... line_idx_seq>
	consteval concat(const char* func, const char* file, ::std::index_sequence<func_idx_seq...>, ::std::index_sequence<file_idx_seq...>, ::std::index_sequence<line_idx_seq...>)
		: str{
			file[file_idx_seq]...,
			' ', ':', ' ',
			get_digit_char<n_digits(line)>(line, line_idx_seq)...,
			' ', '-', ' ',
			func[func_idx_seq]...,
			'\0'
		}
	{}
};

} // namespace jpl::detail

#define JPL_CONCAT_IMPL(A, B) A##B
#define JPL_CONCAT(A, B) JPL_CONCAT_IMPL(A, B)

#define JPL_GET_FILE_FUNC(VAR_NAME)\
static constexpr auto JPL_CONCAT(JPL_FILE_FUNC_, __LINE__) = ::jpl::detail::concat<::jpl::detail::strlen(__FUNCTION__), ::jpl::detail::strlen(__FILE__), __LINE__>(\
	__FUNCTION__, __FILE__,\
	::std::make_index_sequence<::jpl::detail::strlen(__FUNCTION__)>(),\
	::std::make_index_sequence<::jpl::detail::strlen(__FILE__)>(),\
	::std::make_index_sequence<::jpl::detail::n_digits(__LINE__)>()\
);\
static constexpr const char* VAR_NAME = JPL_CONCAT(JPL_FILE_FUNC_, __LINE__).str;

#endif // _MSC_VER
#endif // JPL_GET_FILE_FUNC_HPP
