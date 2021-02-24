#ifndef JPL_ALLOCATOR_HPP
#define JPL_ALLOCATOR_HPP

#include <concepts>
#include <cstdlib>
#include <cstring>

namespace jpl {

template<class Alloc>
concept allocator = requires(Alloc alloc, void* ptr, ::size_t size) {
	{ alloc.allocate(size) } -> std::same_as<void*>;
	{ alloc.reallocate(ptr, size, size) } -> std::same_as<void*>;
	{ alloc.deallocate(ptr, size) };
};

struct mallocator {
	static void* allocate(::size_t size) {
		return ::malloc(size);
	}
	static void* reallocate(void* ptr, ::size_t new_size, ::size_t) {
		return ::realloc(ptr, new_size);
	}
	static void deallocate(void* ptr, ::size_t) {
		::free(ptr);
	}
};

template<::size_t alignment>
struct aligned_allocator {
	static void* allocate(::size_t size) {
		return ::aligned_alloc(alignment, size);
	}
	static void* reallocate(void* old_ptr, ::size_t new_size, ::size_t old_size) {
		void* new_ptr = ::aligned_alloc(alignment, new_size);
		::memcpy(new_ptr, old_ptr, old_size);
		::free(old_ptr);
		return new_ptr;
	}
	static void deallocate(void* ptr, ::size_t) {
		::free(ptr);
	}
};

namespace detail {

template<class T>
struct alloc_selector {
	using type = mallocator;
};

template<class T> requires (alignof(T) > alignof(max_align_t))
struct alloc_selector<T> {
	using type = aligned_allocator<alignof(T)>;
};

} // namespace detail

template<class T>
using default_allocator = typename detail::alloc_selector<T>::type;

} // namespace jpl

#endif // JPL_ALLOCATOR_HPP
