#ifndef JPL_ALLOCATOR_HPP
#define JPL_ALLOCATOR_HPP

#include <concepts>
#include <cstdlib>
#include <cstring>
#include <cstddef>
#include <new>
#include <cstdint>

namespace jpl {

namespace detail {

[[noreturn, gnu::noinline]]
inline void throw_bad_alloc() {
	throw std::bad_alloc{};
}

} // namespace detail

template<class Alloc>
concept allocator = requires(Alloc alloc, void* ptr, ::size_t size) {
	{ alloc.allocate(size) } -> std::same_as<typename Alloc::value_type*>;
	{ alloc.reallocate(ptr, size, size) } -> std::same_as<typename Alloc::value_type*>;
	{ alloc.deallocate(ptr, size) };
};

template<class T>
struct mallocator {
	using value_type = T;
	static T* allocate(::size_t n) {
		if (n >= UINT64_MAX / sizeof(T)) [[unlikely]] detail::throw_bad_alloc();
		void* mem = ::malloc(n * sizeof(T));
		if (!mem) [[unlikely]] detail::throw_bad_alloc();
		return static_cast<T*>(mem);
	}
	static T* reallocate(void* ptr, ::size_t n, ::size_t) {
		if (n >= UINT64_MAX / sizeof(T)) [[unlikely]] detail::throw_bad_alloc();
		void* mem = ::realloc(ptr, n * sizeof(T));
		if (!mem) [[unlikely]] detail::throw_bad_alloc();
		return static_cast<T*>(mem);
	}
	static void deallocate(void* ptr, ::size_t) {
		::free(ptr);
	}
};

template<class T>
struct aligned_allocator {
	using value_type = T;
	static constexpr size_t alignment{ alignof(T) };
	static void* allocate(::size_t n) {
		if (n >= UINT64_MAX / sizeof(T)) [[unlikely]] detail::throw_bad_alloc();
		void* mem = ::aligned_alloc(alignment, n * sizeof(T));
		if (!mem) [[unlikely]] detail::throw_bad_alloc();
		return static_cast<T*>(mem);
	}
	static void* reallocate(void* old_ptr, ::size_t n, ::size_t old_size) {
		if (n >= UINT64_MAX / sizeof(T)) [[unlikely]] detail::throw_bad_alloc();
		void* mem = ::aligned_alloc(alignment, n * sizeof(T));
		if (!mem) [[unlikely]] detail::throw_bad_alloc();
		::memcpy(mem, old_ptr, old_size);
		::free(old_ptr);
		return static_cast<T*>(mem);
	}
	static void deallocate(void* ptr, ::size_t) {
		::free(ptr);
	}
};

namespace detail {

template<class T>
struct alloc_selector {
	using type = mallocator<T>;
};

template<class T> requires (alignof(T) > alignof(max_align_t))
struct alloc_selector<T> {
	using type = aligned_allocator<T>;
};

} // namespace detail

template<class T>
using default_allocator = typename detail::alloc_selector<T>::type;

} // namespace jpl

#endif // JPL_ALLOCATOR_HPP
