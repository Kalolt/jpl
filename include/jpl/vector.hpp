#ifndef JPL_VECTOR_HPP
#define JPL_VECTOR_HPP

#include <jpl/bits/allocator.hpp>
#include <jpl/bits/trivially_relocatable.hpp>
#include <jpl/concepts.hpp>

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <new>
#include <type_traits>
#include <cassert>

// libstdc++'s <iterator> header is insanely bloated, so use internal lighter headers.
#if __has_include(<bits/stl_iterator_base_types.h>) && __has_include(<bits/stl_iterator.h>)
#include <bits/stl_iterator_base_types.h>
#include <bits/stl_iterator.h>
#else
#include <iterator>
#ifdef __GLIBCXX__
#warning "jpl::vector - libstdc++ compile time optimization broken!"
#endif
#endif

namespace jpl {

inline constexpr class list_token_t{} list;
inline constexpr class capacity_t{}   capacity;
inline constexpr class sic_t{}        sic;

namespace detail {
template<class value_type, ::size_t sbo_size>
union vector_union {
	constexpr vector_union() noexcept {}
	constexpr ~vector_union() noexcept {}
	value_type sbo_[sbo_size];
	value_type* end_;
};
template<class value_type>
union vector_union<value_type, 0> {
	constexpr vector_union() noexcept {}
	constexpr ~vector_union() noexcept {}
	value_type* end_;
};

} // namespace detail

template<class T, ::size_t sbo_size_ = 0, ::jpl::allocator A = ::jpl::default_allocator<T>>
class vector {
	static_assert(::std::is_same_v<T, ::std::remove_cvref_t<T>>, "::jpl::vector's value type must not be const, volatile, or reference");

	static constexpr bool is_trivial = ::std::is_trivial_v<T>;
	// When constructing elements from a base value, take trivial values by copy, others by const reference
	using copy_t = ::std::conditional_t<::std::is_trivially_copyable_v<T>, T, const T&>;

	public:
	using value_type             = T;
	using size_type              = ::size_t;
	using difference_type        = ::ptrdiff_t;
	using referece               = T&;
	using const_referece         = const T&;
	using pointer                = T*;
	using const_pointer          = const T*;
	// TODO: use C++20's std::contiguous_iterator, when all major compilers support it.
	using iterator               = T*;
	using const_iterator         = const T*;
	using reverse_iterator       = ::std::reverse_iterator<T*>;
	using const_reverse_iterator = ::std::reverse_iterator<const T*>;
	using allocator              = A;
	
	static constexpr ::size_t sbo_size = sbo_size_;

	template<::size_t sbo_size_other>
	using vector_other = vector<T, sbo_size_other, A>;

	static constexpr ::size_t alignment = alignof(T);

	// Make all specializations friends with each other
	// This lets private elements be accessed in move/copy operations between different SBO sizes
	template<class T_, ::size_t, ::jpl::allocator>
	friend class vector;

	constexpr vector() noexcept
		: beg_{ nullptr }, end_{ nullptr }
	{
		if constexpr (sbo_size == 0) {
			data_.end_ = nullptr;
		} else {
			beg_ = data_.sbo_;
			end_ = beg_;
		}
	}

	vector(const size_type n) requires (is_trivial)
		: beg_{ allocate(n) }, end_{ beg_ + n }
	{
		if (!is_sbo_active()) data_.end_ = beg_ + n;
	}

	vector(const size_type n) requires (!is_trivial)
		: beg_{ allocate(n) }, end_{ beg_ }
	{
		try {
			while (end_ != beg_ + n)
				::new (end_++) T;
		} catch (...) {
			end_--;
			while (end_ != beg_)
				(--end_)->~T();
			throw;
		}
		if (!is_sbo_active()) data_.end_ = beg_ + n;
	}

	vector(const capacity_t, const size_type n)
		: beg_{ allocate(n) }, end_{ beg_ }
	{
		if (!is_sbo_active()) data_.end_ = beg_ + n;
	}

	template<class F> requires ::std::is_invocable_r_v<T, F>
	vector(const size_type n, F&& gen)
		: beg_{ allocate(n) }, end_{ beg_ }
	{
		try {
			while (end_ != beg_ + n)
				::new (end_++) T{ gen() };
		} catch (...) {
			end_--;
			while (end_ != beg_)
				(--end_)->~T();
			throw;
		}
		if (!is_sbo_active()) data_.end_ = beg_ + n;
	}

	template<class F> requires ::std::is_invocable_r_v<T, F, size_type>
	vector(size_type n, F&& gen)
		: beg_{ allocate(n) }, end_{ beg_ }
	{
		try {
			for (::size_t i = 0; i < n; i++)
				::new (end_++) T{ gen(i) };
		} catch (...) {
			end_--;
			while (end_ != beg_)
				(--end_)->~T();
			throw;
		}
		if (!is_sbo_active()) data_.end_ = beg_ + n;
	}

	vector(size_type n, copy_t base)
		: beg_{ allocate(n) }, end_{ beg_ }
	{
		if constexpr (is_trivial && (sizeof(T) == 1)) {
			if constexpr (::std::is_same_v<unsigned char, T> || ::std::is_same_v<char, T>) {
				::memset(beg_, base, n);
			} else {
				// This is for types like std::byte and wrappers around 1 byte types
				unsigned char c;
				::memcpy(&c, &base, 1);
				::memset(beg_, c, n);
			}
			end_ = beg_ + n;
		} else {
			try {
				while (end_ != beg_ + n)
					::new (end_++) T{ base };
			} catch (...) {
				end_--;
				while (end_ != beg_)
					(--end_)->~T();
				throw;
			}
		}
		if (!is_sbo_active()) data_.end_ = beg_ + n;
	}

	template<class ... Args> requires ((::std::is_constructible_v<value_type, Args> && ...) || (sizeof...(Args) == 0))
	vector(list_token_t, Args&& ... args)
		: beg_{ allocate(sizeof...(args)) }, end_{ beg_ }
	{
		try {
			(::new (end_++) T{ static_cast<Args&&>(args) }, ...);
		} catch (...) {
			end_--;
			while (end_ != beg_)
				(--end_)->~T();
			throw;
		}
		if (!is_sbo_active()) data_.end_ = end_;
	}

	vector(const vector& other) requires(!is_trivial)
		: beg_{ allocate(other.size()) }, end_{ beg_ }
	{
		try {
			for (const_iterator it = other.cbegin(); it != other.cend(); ++it)
				::new (end_++) T{ *it };
		} catch (...) {
			end_--;
			while (end_ != beg_)
				(--end_)->~T();
			throw;
		}
		if (!is_sbo_active()) data_.end_ = end_;
	};

	vector(const vector& other) requires(is_trivial)
		: beg_{ allocate(other.size()) }, end_{ beg_ + other.size() }
	{
		if (other.beg_)
			::memcpy(beg_, other.beg_, other.size_bytes());
		if (!is_sbo_active()) data_.end_ = end_;
	};

	// Hide implicits to use generic versions of these
	vector(vector&&) requires(false) {};
	vector& operator=(vector&&) requires(false) {};
	vector& operator=(const vector&) requires(false) {};

	// Construct from another range (per-element move or copy).
	// This also handles copy-construction from another ::jpl::vector.
	// TODO: using memcpy for trivial types could potentially make this faster, but implementing it is a mess,
	// because the other range needs to be contiguous, and ::std::contiguous_iterator_tag seems to be used inconsistently.
	template<class U> requires (convertible_range<U, value_type>)
	vector(U&& range)
		: beg_{ allocate(range.size()) }, end_{ beg_ }
	{
		try {
			for (auto it = range.begin(); it != range.end(); ++it) {
				if constexpr (::std::is_rvalue_reference_v<U>)
					::new (end_++) T{ static_cast<::std::remove_reference_t<decltype(*it)>&&>(*it) };
				else
					::new (end_++) T{ *it };
			}
		} catch (...) {
			end_--;
			while (end_ != beg_)
				(--end_)->~T();
			throw;
		}
		if (!is_sbo_active()) data_.end_ = end_;
	}

	// Move from another ::jpl::vector.
	// If the other vector's data isn't currently SBO'd, transfer heap directly.
	template<::size_t sbo>
	vector(vector_other<sbo>&& other) noexcept(sbo == 0) {
		const ::size_t size_ = other.size();
		if (other.is_sbo_active() || (sbo_size >= size_)) {
			if constexpr (::jpl::trivially_relocatable<T>) {
				beg_ = allocate(size_);
				end_ = beg_ + size_;
				::memcpy(beg_, other.beg_, size_ * sizeof(T));
			} else {
				end_ = beg_ = allocate(size_);
				try {
					T* it = other.beg_;
					while (other.beg_ != other.end_) {
						::new (end_++) T{ static_cast<T&&>(*(it++)) };
						other.beg_->~T();
					}
					other.end_ = other.beg_;
				} catch (...) {
					end_--;
					while (end_ != beg_)
						(--end_)->~T();
					throw;
				}
			}
			if (!is_sbo_active()) data_.end_ = end_;
		} else {
			beg_ = other.beg_;
			end_ = other.end_;
			data_.end_ = other.data_.end_;
			other.beg_ = nullptr;
			other.end_ = nullptr;
			other.data_.end_ = nullptr;
		}
	}
	
	template<::size_t sbo>
	vector& operator=(const vector_other<sbo>& other) {
		const size_type n{ other.size() };
		if (n > capacity()) {
			dtor_range(beg_, end_);
			if (!is_sbo_active()) alloc.deallocate(beg_, capacity() * sizeof(T));
			end_ = beg_ = static_cast<T*>(alloc.allocate(n * sizeof(T)));
			if constexpr (is_trivial) {
				::memcpy(beg_, other.beg_, other.size_bytes());
				end_ = beg_ + other.size();
			} else {
				const_iterator it{ other.cbegin() };
				while (it != other.cend())
					::new (end_++) T{ *it++ };
			}
			data_.end_ = end_;
		} else if (n > size()) {
			const T* src = other.cbegin();
			const T* end = other.cbegin() + size(); // Not the same as other.end()!!!
			T* dst = beg_;
			while (src != end)
				*dst++ = *src++;
			const_iterator it{ other.cbegin() + size() };
			while (it != other.cend())
				::new (end_++) T{ *it++ };
		} else {
			dtor_range(beg_ + n, end_);
			const T* src = other.cbegin();
			const T* end = other.cend();
			T* dst = beg_;
			while (src != end)
				*dst++ = *src++;
			end_ = beg_ + n;
		}
		return *this;
	}

	template<::size_t sbo>
	vector& operator=(vector_other<sbo>&& other) noexcept(sbo == 0) {
		if (other.is_sbo_active()) {
			const size_type n{ other.size() };
			if (n > capacity()) {
				dtor_range(beg_, end_);
				if (!is_sbo_active()) alloc.deallocate(beg_, capacity() * sizeof(T));
				end_ = beg_ = static_cast<T*>(alloc.allocate(n * sizeof(T)));
				for (iterator it{ other.beg_ }; it != other.end_;) {
					::new (end_++) T{ static_cast<T&&>(*it) };
					(it++)->~T();
				}
				other.end_ = other.beg_;
				if (!is_sbo_active()) data_.end_ = end_;
			} else if (n > size()) {
				iterator it = other.begin();
				for (iterator val = beg_; val != end_; val++) {
					*val = static_cast<T&&>(*it);
					(it++)->~T();
				}
				while (it != other.cend()) {
					::new (end_++) T{ static_cast<T&&>(*it) };
					(it++)->~T();
				}
				other.end_ = other.beg_;
			} else {
				dtor_range(beg_ + n, end_);
				beg_ = end_ = beg_ + n;
				while (other.beg_ != other.end_) {
					*(--beg_) = static_cast<T&&>(*(--other.end_));
					(other.end_)->~T();
				}
			}
		} else {
			dtor_range(beg_, end_);
			if (!is_sbo_active()) alloc.deallocate(beg_, capacity() * sizeof(T));
			beg_ = other.beg_;
			end_ = other.end_;
			data_.end_ = other.data_.end_;
			other.beg_ = nullptr;
			other.end_ = nullptr;
			other.data_.end_ = nullptr;
		}
		return *this;
	}

	~vector() noexcept(noexcept(::std::declval<T>().~T())) {
		if constexpr (sbo_size == 0) {
			dtor_range(beg_, end_);
			alloc.deallocate(beg_, capacity() * sizeof(T));
		} else {
			dtor_range(beg_, end_);
			if (!is_sbo_active()) alloc.deallocate(beg_, capacity() * sizeof(T));
		}
	}

	// Element access
	[[nodiscard]] referece       operator[](size_type pos)       noexcept { assert(pos < size()); return beg_[pos]; }
	[[nodiscard]] const_referece operator[](size_type pos) const noexcept { assert(pos < size()); return beg_[pos]; }
	[[nodiscard]] referece       front()                         noexcept { assert(!empty());     return *beg_; }
	[[nodiscard]] const_referece front()                   const noexcept { assert(!empty());     return *beg_; }
	[[nodiscard]] referece       back()                          noexcept { assert(!empty());     return *(end_ - 1); }
	[[nodiscard]] const_referece back()                    const noexcept { assert(!empty());     return *(end_ - 1); }
	[[nodiscard]] pointer        data()                          noexcept { return beg_; }
	[[nodiscard]] const_pointer  data()                    const noexcept { return beg_; }

	// Iterators
	[[nodiscard]] iterator               begin()         noexcept { return beg_; }
	[[nodiscard]] const_iterator         begin()   const noexcept { return beg_; }
	[[nodiscard]] const_iterator         cbegin()  const noexcept { return beg_; }
	[[nodiscard]] iterator               end()           noexcept { return end_; }
	[[nodiscard]] const_iterator         end()     const noexcept { return end_; }
	[[nodiscard]] const_iterator         cend()    const noexcept { return end_; }
	[[nodiscard]] reverse_iterator       rbegin()        noexcept { return reverse_iterator      { end_ }; }
	[[nodiscard]] const_reverse_iterator rbegin()  const noexcept { return const_reverse_iterator{ end_ }; }
	[[nodiscard]] const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator{ end_ }; }
	[[nodiscard]] reverse_iterator       rend()          noexcept { return reverse_iterator      { beg_ }; }
	[[nodiscard]] const_reverse_iterator rend()    const noexcept { return const_reverse_iterator{ beg_ }; }
	[[nodiscard]] const_reverse_iterator crend()   const noexcept { return const_reverse_iterator{ beg_ }; }

	// Capacity
	[[nodiscard]] bool empty()          const noexcept { return beg_ == end_; }
	[[nodiscard]] ::size_t size()       const noexcept { return end_ - beg_; }
	[[nodiscard]] ::size_t size_bytes() const noexcept { return size() * sizeof(value_type); }
	[[nodiscard]] ::size_t capacity()   const noexcept { return is_sbo_active() ? sbo_size : (data_.end_ - beg_); }

	vector& clear() noexcept(noexcept(::std::declval<T>().~T())) {
		dtor_range(beg_, end_);
		end_ = beg_;
		return *this;
	}

	constexpr iterator erase(const_iterator pos) {
		assert(pos >= beg_ && pos < end_);
		pos->~T();
		--end_;

		T* it = const_cast<T*>(pos);
		if constexpr (::jpl::trivially_relocatable<T>) {
			::std::memmove(it, it + 1, (end_ - it) * sizeof(T));
		} else {
			while (it != end_) {
				*it = static_cast<T&&>(*(it + 1));
				++it;
			}
			end_->~T();
		}

		return const_cast<T*>(pos);
	}

	constexpr iterator erase(const_iterator first, const_iterator last) {
		assert(first >= beg_ && last < end_);
		dtor_range(const_cast<T*>(first), const_cast<T*>(last));
		const ::ptrdiff_t dist = last - first;
		end_ -= dist;

		T* it = const_cast<T*>(first);
		if constexpr (::jpl::trivially_relocatable<T>) {
			::std::memmove(it, it + dist, (end_ - last + dist) * sizeof(T));
		} else {
			while (it != end_) {
				*it = static_cast<T&&>(*(it + dist));
				++it;
			}
			while (it != (end_ + dist))
				(it++)->~T();
		}

		return const_cast<T*>(first);
	}

	void reserve(size_type new_size) {
		if (new_size > size()) [[likely]]
			grow_to_size(new_size);
	}

	void reserve(sic_t, size_type new_capacity) {
		::size_t size_ = size();
		assert(new_capacity >= size_ && "jpl::vector trying to reserve capacity that doesn't fit data");
		if constexpr (::jpl::trivially_relocatable<T>) {
			if (is_sbo_active()) {
				T* new_buffer{ static_cast<T*>(alloc.allocate(new_capacity * sizeof(T))) };
				::memcpy(new_buffer, beg_, size_ * sizeof(T));
				beg_ = new_buffer;
			}	else {
				// beg_ could be nullptr, but then realloc works like malloc
				beg_ = static_cast<T*>(alloc.reallocate(beg_, new_capacity * sizeof(T), capacity() * sizeof(T)));
			}
			end_ = beg_ + size_;
		} else {
			T* new_buffer{ static_cast<T*>(alloc.allocate(new_capacity * sizeof(T))) };
			T* old_end{ end_ };
			end_ = new_buffer;
			for (iterator it{ beg_ }; it != old_end; it++) {
				::new (end_++) T{ static_cast<T&&>(*it) };
				(it)->~T();
			}
			if (!is_sbo_active()) alloc.deallocate(beg_, capacity() * sizeof(T));
			beg_ = new_buffer;
		}
		data_.end_ = beg_ + new_capacity;
	}

	vector& shrink_to_fit() {
		if (!is_sbo_active() && (end_ != data_.end_)) [[likely]] {
			::size_t size_ = size();
			if (size_ == 0) {
				alloc.deallocate(beg_, capacity() * sizeof(T));
				if constexpr (sbo_size == 0) {
					beg_ = nullptr;
					end_ = nullptr;
					data_.end_ = nullptr;
				} else {
					beg_ = ::std::addressof(data_.sbo_[0]);
					end_ = beg_;
				}
			} else {
				T* new_beg = allocate(size_);
				if constexpr (::jpl::trivially_relocatable<T>) {
					::memcpy(new_beg, beg_, size_ * sizeof(T));
				} else {
					T* it = new_beg + size_;
					while (it != new_beg) {
						new (--it) T{ static_cast<T&&>(*--end_) };
						end_->~T();
					}
				}
				alloc.deallocate(beg_, capacity() * sizeof(T));
				beg_ = new_beg;
				end_ = beg_ + size_;
				if (!is_sbo_active()) data_.end_ = end_;
			}
		}
		return *this;
	};

	vector& push_back(const T& value) {
		emplace_back(value);
		return *this;
	}
	vector& push_back(T&& value) {
		emplace_back(static_cast<T&&>(value));
		return *this;
	}

	template<class U>
	iterator insert_impl(iterator pos, U&& value) {
		/*
			TODO: create a specialized version of grow_if_full that doesn't use reserve,
			because currently some elements are moved twice when vector grows.
		*/
		auto dif = pos - beg_;
		grow_if_full();
		::new (end_) T{ static_cast<T&&>(*(end_ - 1)) };

		T* src = end_ - 1;
		T* dst = end_;
		T* end = beg_ + dif;

		while (src != end) {
			*(--dst) = static_cast<T&&>(*(--src));
		}

		end_++;
		beg_[dif] = static_cast<U&&>(value);
		return beg_ + dif;
	}

	iterator insert(iterator pos, const value_type& value) {
		if (pos == end_)
			return ::std::addressof(emplace_back(value));
		return insert_impl(pos, value);
	}

	iterator insert(iterator pos, value_type&& value) {
		if (pos == end_)
			return ::std::addressof(emplace_back(static_cast<T&&>(value)));
		return insert_impl(pos, static_cast<T&&>(value));
	}

	template<class ... Args>
	referece emplace_back(Args&& ... args) {
		grow_if_full();
		::new (end_) T{ static_cast<Args&&>(args)... };
		return *end_++;
	}

	vector& append(iterator begin, iterator end) {
		grow_if_full(end - begin);
		while (begin != end)
			::new (end_++) T{ static_cast<T&&>(*begin++) };
		return *this;
	}

	vector& append(const_iterator begin, const_iterator end) {
		grow_if_full(end - begin);
		while (begin != end)
			::new (end_++) T{ *begin++ };
		return *this;
	}

	template<class U> requires (convertible_range<U, value_type>)
	vector& append(U&& rng)
	{
		if constexpr (::std::is_rvalue_reference_v<U>)
			return append(rng.begin(), rng.end());
		else
			return append(rng.cbegin(), rng.cend());
	}

	template<class U> requires (convertible_range<U, value_type> || can_add<value_type, U>)
	friend auto operator+(const vector& left, U&& right) {
		if constexpr (convertible_range<U, value_type>) {
			vector<value_type, 0> res{ ::jpl::capacity, left.size() + right.size() };
			res += left;
			return res += right;
		} else {
			vector res{ left };
			return res += right;
		}
	}

	template<class U> requires (convertible_range<U, value_type> || can_add<U, value_type>)
	vector& operator+=(U&& value) {
		if constexpr (convertible_range<U, value_type>) {
			return append(static_cast<U&&>(value));
		} else {
			for (size_type i{ 0 }; i < size(); ++i)
				beg_[i] += value;
			return *this;
		}
	}

	template<class U> requires (can_reduce<value_type, U>)
	friend vector operator-(vector left, U&& right) {
		return left -= right;
	}

	template<class U> requires (can_reduce<value_type, U>)
	vector& operator-=(U&& value) {
		for (size_type i{ 0 }; i < size(); ++i)
			beg_[i] -= value;
		return *this;
	}

	void pop_back() {
		(--end_)->~T();
	}

	private:
	[[gnu::always_inline]]
	void resize_impl(size_type new_size) {
		if constexpr (!is_trivial) {
			if (new_size > size())
				while (end_ != beg_ + new_size)
					::new (end_++) T;
			else
				dtor_range(beg_ + new_size, end_);
		}
		end_ = beg_ + new_size;
	}

	[[gnu::always_inline]]
	void resize_impl(size_type new_size, copy_t val) {
		if (new_size > size()) {
			if constexpr (is_trivial && (sizeof(T) == 1)) {
				::size_t n = new_size - size();
				if constexpr (::std::is_same_v<unsigned char, T> || ::std::is_same_v<char, T>) {
					::memset(end_, val, n);
				} else {
					unsigned char c;
					::memcpy(&c, &val, 1);
					::memset(end_, c, n);
				}
				end_ += n;
			} else {
				while (end_ != beg_ + new_size)
					::new (end_++) T{ val };
			}
		}
		else
			dtor_range(beg_ + new_size, end_);
		end_ = beg_ + new_size;
	}

	template<class F> [[gnu::always_inline]]
	void resize_impl(size_type new_size, const F& gen) {
		if (new_size > size()) {
			if constexpr (::std::is_invocable_r_v<T, F, size_type>) {
				for (size_type i{ size() }; i < new_size; ++i)
					::new (end_++) T{ gen(i) };
			} else {
				while (end_ != beg_ + new_size)
					::new (end_++) T{ gen() };
			}
		} else {
			dtor_range(beg_ + new_size, end_);
		}
		end_ = beg_ + new_size;
	}

	public:

	void resize(size_type new_size) {
		if (new_size > capacity())
			grow_to_size(new_size);
		resize_impl(new_size);
	}
	void resize(sic_t, size_type new_size) {
		if (new_size > capacity())
			reserve(sic, new_size);
		resize_impl(new_size);
	}

	void resize(size_type new_size, const T& val) {
		if (new_size > capacity())
			grow_to_size(new_size);
		resize_impl(new_size, val);
	}
	void resize(sic_t, size_type new_size, const T& val) {
		if (new_size > capacity())
			reserve(sic, new_size);
		resize_impl(new_size, val);
	}

	template<class F> requires (invocable_r<T, F, size_type> || invocable_r<T, F>)
	void resize(size_type new_size, const F& gen) {
		if (new_size > capacity())
			grow_to_size(new_size);
		resize_impl(new_size, gen);
	}
	template<class F> requires (invocable_r<T, F, size_type> || invocable_r<T, F>)
	void resize(sic_t, size_type new_size, const F& gen) {
		if (new_size > capacity())
			reserve(sic, new_size);
		resize_impl(new_size, gen);
	}

	template<class U> requires (can_alias<value_type, U>)
	operator const U*() const noexcept {
		return reinterpret_cast<const U*>(data());
	}
	template<class U> requires (can_alias<value_type, U>)
	operator U*() noexcept {
		return reinterpret_cast<U*>(data());
	}

	[[gnu::always_inline]]
	constexpr bool is_sbo_active() const noexcept {
		if constexpr (sbo_size == 0)
			return false;
		else
			return beg_ == ::std::addressof(data_.sbo_[0]);
	}

	constexpr void swap(vector& other) noexcept {
		if (sbo_size == 0 || (!is_sbo_active() && !other.is_sbo_active())) {
			T* tmp = beg_;
			beg_ = other.beg_;
			other.beg_ = tmp;
			tmp = end_;
			end_ = other.end_;
			other.end_ = tmp;
			tmp = data_.end_;
			data_.end_ = other.data_.end_;
			other.data_.end_ = tmp;
		} else {
			vector temp{ static_cast<vector&&>(*this) };
			*this = static_cast<vector&&>(other);
			other = temp;
		}
	}

	friend constexpr void swap(vector& lhs, vector& rhs) noexcept {
		lhs.swap(rhs);
	}

	private:
	[[gnu::always_inline]]
	constexpr void dtor_range(T* begin, T* end) noexcept {
		// If vector was iterated through before it went out of scope, which is often the case,
		// elements at the end are more likely to be cached, thus destroying in reverse order can lead to fewer cache misses
		if constexpr (!::std::is_trivially_destructible_v<T>)
			while (begin != end) (--end)->~T();
	}

	[[gnu::always_inline]]
	T* allocate(typename ::size_t n) {
		if constexpr (sbo_size == 0) {
			return static_cast<T*>(alloc.allocate(n * sizeof(T)));
		} else {
			if (n > sbo_size)
				return static_cast<T*>(alloc.allocate(n * sizeof(T)));
			else
				return ::std::addressof(data_.sbo_[0]);
		}
	}

	[[gnu::always_inline]]
	T* data_end() const noexcept {
		return is_sbo_active() ? (beg_ + sbo_size) : data_.end_;
	}

	[[gnu::always_inline]]
	void grow_if_full() {
		if (data_end() == end_) reserve(sic, size() ? size() * 2 : 1);
	}

	[[gnu::always_inline]]
	void grow_if_full(::size_t n) {
		if (data_end() < (end_ + n)) grow_to_size(size() + n);
	}

	void grow_to_size(::size_t n) {
		const size_type new_req{ n };
		const size_type old_dbl{ size() * 2 };
		reserve(::jpl::sic, new_req > old_dbl ? new_req : old_dbl);
	}
	
	T* beg_;
	T* end_;
	detail::vector_union<value_type, sbo_size> data_;
	[[no_unique_address]] A alloc;
};

// jpl::vector is trivially relocatable, if it doesn't use SBO, and it uses a trivially relocatable allocator
// stateless allocators should be naturally trivial, and thus automatically trivially_relocatable
template<class T, class A> requires (::jpl::trivially_relocatable<A>)
inline constexpr bool trivially_relocatable<vector<T, 0, A>> = true;

} // namespace jpl

#endif // JPL_VECTOR_HPP
