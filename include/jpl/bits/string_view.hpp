/* This might be a bad idea. */

#ifndef JPL_STRING_VIEW_HPP
#define JPL_STRING_VIEW_HPP

#include <cstddef>
#include <cstring>

namespace jpl {

class string_view {
	const char* ptr;
	::size_t size_;

public:
	string_view(const char* str)                noexcept : ptr{ str }, size_{ ::strlen(str) } {}
	string_view(const char* str, ::size_t size) noexcept : ptr{ str }, size_{ size } {}

	[[gnu::always_inline]] const char*  data () const noexcept { return ptr; }
	[[gnu::always_inline]] const char*  begin() const noexcept { return ptr; }
	[[gnu::always_inline]] const char*  end  () const noexcept { return ptr + size_; }
	[[gnu::always_inline]]     ::size_t size () const noexcept { return size_; }

	//operator const char*() const noexcept { return ptr; }
};

} // namespace jpl

#endif // JPL_STRING_VIEW_HPP
