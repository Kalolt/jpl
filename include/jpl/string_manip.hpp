#ifndef JPL_STRING_MANIP_HPP
#define JPL_STRING_MANIP_HPP

#include <cstring>
#include <string>
#include <jpl/vector.hpp>
#include <jpl/bits/invoke.hpp>

namespace jpl {

template<class T> requires(::std::same_as<T, ::std::string> || ::std::same_as<T, ::std::string_view>)
[[nodiscard]] inline ::std::string concat(const ::jpl::vector<T>& strs, ::std::string_view separator = ::std::string_view{ "" }) {
	if (strs.size() == 0) return "";

	::size_t size{ 0 };
	for (const auto str : strs)
		size += str.size();
	size += (strs.size() - 1) * separator.size();

	::std::string res( size, '\0' );
	char* start = res.data();
	char* end   = res.data() + res.size();
	for (const auto str : strs) {
		::memcpy(start, str.data(), str.size());
		start += str.size();
		if (start != end) {
			::memcpy(start, separator.data(), separator.size());
			start += separator.size();
		}
	}

	return res;
}

template<class T>
[[nodiscard]] inline ::std::string concat(const T& strs, ::std::string_view separator = ::std::string_view{ "" }) {
	::jpl::vector<::std::string_view> str_views;
	for (auto& str : strs)
		str_views.emplace_back(str);
	return concat(str_views, separator);
}

template<class T, class Proj = ::std::string_view(*)(const typename T::value_type&)>
[[nodiscard]] inline ::std::string concat(const T& strs, const Proj& projection, ::std::string_view separator = ::std::string_view{ "" }) {
	::jpl::vector<::std::string_view> str_views;
	for (auto& str : strs)
		str_views.emplace_back(jpl::invoke(projection, str));
	return concat(str_views, separator);
}

template<class T>
[[nodiscard]] inline ::std::string concat(const ::std::initializer_list<T>& strs, ::std::string_view separator = ::std::string_view{ "" }) {
	::jpl::vector<::std::string_view> str_views;
	for (auto& str : strs)
		str_views.emplace_back(str);
	return concat(str_views, separator);
}

template<class T, class Proj = ::std::string_view(*)(const typename T::value_type&)>
[[nodiscard]] inline ::std::string concat(const ::std::initializer_list<T>& strs, const Proj& projection, ::std::string_view separator = ::std::string_view{ "" }) {
	::jpl::vector<::std::string_view> str_views;
	for (auto& str : strs)
		str_views.emplace_back(jpl::invoke(projection, str));
	return concat(str_views, separator);
}

[[nodiscard]] inline ::size_t count_matches(::std::string_view str, ::std::string_view substr) {
	::size_t res = 0;
	::size_t offset = 0;
	while ((offset = str.find(substr, offset)) != ::std::string_view::npos) {
		offset += substr.size();
		res++;
	}
	return res;
}

[[nodiscard]] inline ::std::string replace_all(::std::string_view input, ::std::string_view match_str, ::std::string_view replace_str) {
	::size_t n_matches = count_matches(input, match_str);
	if (n_matches == 0) return ::std::string{ input };
	int size_difference = replace_str.size() - match_str.size();

	::std::string res( input.size() + size_difference * n_matches, '\0' );
	char* start = res.data();
	::size_t offset = 0;
	::size_t pos;
	while ((pos = input.find(match_str, offset)) != ::std::string_view::npos) {
		::memcpy(start, input.data() + offset, pos - offset);
		start += pos - offset;
		::memcpy(start, replace_str.data(), replace_str.size());
		start += replace_str.size();
		offset = pos + match_str.size();
	}
	::memcpy(start, input.data() + offset, input.size() - offset);

	return res;
}

inline void replace_all(::std::string& input, ::std::string_view match_str, ::std::string_view replace_str) {
	::size_t n_matches = count_matches(input, match_str);
	if (n_matches == 0) return;
	int size_difference = replace_str.size() - match_str.size();

	::size_t offset = 0;
	::size_t pos;
	if (size_difference == 0) {
		// match_str and replace_str are the same size - replacement can be done trivially in-place
		while ((pos = input.find(match_str, offset)) != ::std::string::npos) {
			::memcpy(input.data() + pos, replace_str.data(), replace_str.size());
			offset = pos + match_str.size();
		}
	} else if (size_difference > 0) {
		// If replacte_str is larger than match_str, a new buffer will likely need to be allocated, anyway.
		// In this case trying to do the operation in-place is costlier, because parts of the string need to be moved twice.
		// Therefore it makes more sense to simply create a new string, generate the result there, and swap it with the input.
		::std::string tmp( input.size() + size_difference * n_matches, '\0' );
		char* move_to = tmp.data();
		while ((pos = input.find(match_str, offset)) != ::std::string_view::npos) {
			::memcpy(move_to, input.data() + offset, pos - offset);
			move_to += pos - offset;
			::memcpy(move_to, replace_str.data(), replace_str.size());
			move_to += replace_str.size();
			offset = pos + match_str.size();
		}
		::memcpy(move_to, input.data() + offset, input.size() - offset);
		input.swap(tmp);

		/* Alternative in-place solution, which might be faster with an alternative string implementation that uses realloc.

		// replace_str is longer than match_str, so a bigger buffer is needed
		::size_t orig_size = input.size();
		input.resize(input.size() + size_difference * n_matches);

		// First find the first match, and move everything after it to the end of the string
		pos = input.find(match_str); // guaranteed to not be npos, because n_matches can't be 0
		char* move_from = input.data() + pos + match_str.size();
		char* move_to   = move_from + size_difference * n_matches;

		::memmove(move_to, move_from, orig_size - pos);
		::memcpy(input.data() + pos, replace_str.data(), replace_str.size());

		move_to   = input.data() + pos + replace_str.size();
		move_from = move_from + size_difference * n_matches;
		offset    = move_from - input.data();

		// In the loop move substrings from the back and copy replacement strings until done
		while ((pos = input.find(match_str, offset)) != ::std::string::npos) {
			::memmove(move_to, move_from, pos - offset);
			move_to += pos - offset;
			::memcpy(move_to, replace_str.data(), replace_str.size());
			move_to += replace_str.size();
			offset = pos + match_str.size();
		}
		*/
	} else if (size_difference < 0) {
		// replace_str is shorter than match_str, so string can be manipulated in place, and shrunk at the end
		pos = input.find(match_str); // guaranteed to not be npos, because n_matches can't be 0
		::memcpy(input.data() + pos, replace_str.data(), replace_str.size());

		char* move_to = input.data() + pos + replace_str.size();
		offset = pos + match_str.size();

		while ((pos = input.find(match_str, offset)) != ::std::string::npos) {
			::memmove(move_to, input.data() + offset, pos - offset);
			move_to += pos - offset;
			::memcpy(move_to, replace_str.data(), replace_str.size());
			move_to += replace_str.size();
			offset = pos + match_str.size();
		}
		::memmove(move_to, input.data() + offset, input.size() - offset);

		input.resize(input.size() + size_difference * n_matches);
	}
}

inline void trim_front(::std::string_view& str, ::std::string_view what = ::std::string_view{ " \t\n\r" }) {
	if (auto pos = str.find_first_not_of(what); pos != ::std::string_view::npos)
		str.remove_prefix(pos);
}

inline void trim_back(::std::string_view& str, ::std::string_view what = ::std::string_view{ " \t\n\r" }) {
	if (auto pos = str.find_last_not_of(what); pos != ::std::string_view::npos)
		str.remove_suffix(str.size() - pos - 1);
}

inline void trim(::std::string_view& str, ::std::string_view what = ::std::string_view{ " \t\n\r" }) {
	trim_front(str, what);
	trim_back (str, what);
}

} // namespace jpl

#endif // JPL_STRING_MANIP_HPP
