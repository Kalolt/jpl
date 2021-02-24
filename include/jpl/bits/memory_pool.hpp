/*
	Finite static memory pool for quick allocs.
	Uses new/delete, if allocated over capacity.
*/

#ifndef JPL_MEMORY_POOL_HPP
#define JPL_MEMORY_POOL_HPP

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <new>
#include <jpl/bits/assert_thread.hpp>
#include <mutex>

namespace jpl {

namespace detail {

// Smallest integer type to fit capacity
template<::size_t capacity>
struct smallest { using type = ::uint64_t; };
template<::size_t capacity> requires(UINT32_MAX >= capacity && UINT16_MAX < capacity)
struct smallest<capacity> { using type = ::uint32_t; };
template<::size_t capacity> requires(UINT16_MAX >= capacity && UINT8_MAX  < capacity)
struct smallest<capacity> { using type = ::uint16_t; };
template<::size_t capacity> requires(UINT8_MAX >= capacity)
struct smallest<capacity> { using type = ::uint8_t; };

} // namespace detail

template<class T, ::size_t capacity>
class static_memory_pool {
	using idx_t = typename detail::smallest<capacity>::type;
	union element {
		idx_t idx;
		T f;
		constexpr  element() noexcept {}
		~element() noexcept {}
	};
	
	//JPL_ASSERT_ATOM(safe);
	::std::mutex mtx;
	element mem[capacity];

	public:
	constexpr static_memory_pool() noexcept {
		for (idx_t i = 1; auto& e : mem)
			e.idx = i++;
		mem[capacity - 1].idx = 0;
	}

	template<class ... Args>
	T* alloc(Args&& ... args) noexcept {
		//JPL_ASSERT_THREAD(safe);
		::std::scoped_lock lock{ mtx };
		idx_t idx = mem[0].idx;
		if (idx) [[likely]] {
			mem[0].idx = mem[idx].idx;
			return ::new (&(mem[idx].f)) T{ static_cast<Args&&>(args)... };	
		} else {
			return ::new T{ static_cast<Args&&>(args)... };
		}
	}

	void free(T* ptr) noexcept {
		//JPL_ASSERT_THREAD(safe);
		::std::scoped_lock lock{ mtx };
		const void* ptr_add = ptr;
		const void* beg = mem;
		const void* end = mem + capacity;
		if ((ptr_add >= beg) && (ptr_add < end)) [[likely]] {
			idx_t idx = reinterpret_cast<element*>(ptr) - &mem[0];
			ptr->~T();
			mem[idx].idx = mem[0].idx;
			mem[0].idx = idx;
		} else {
			delete ptr;
		}
	};
};

} // namespace jpl

#endif // JPL_MEMORY_POOL_HPP
