#ifndef JPL_BITS_CALLABLE_HPP
#define JPL_BITS_CALLABLE_HPP

#include <cstddef>
#include <type_traits>
#include <cstring>

namespace jpl::detail {

static_assert(sizeof(void*) == sizeof(void(*)()) && alignof(void*) == alignof(void(*)()),
	"jpl::function requires that function pointers have same size and alignment as normal pointers.");

struct dummy_struct;
union pointer_types_member {
	void* pointer;
	void (*function_pointer)();
	void (dummy_struct::*member_pointer)();
};

template<::size_t local_storage_size>
struct [[gnu::may_alias]] callable_u {
	union pointer_types {
		void* pointer;
		void (*function_pointer)();
	};

	static constexpr struct uninitialized_t{} uninitialized{};

	// Ignore requested local_storage_size, if it can't fit a pointer
	static constexpr ::size_t real_size = local_storage_size > sizeof(pointer_types) ? local_storage_size : sizeof(pointer_types);

	// If local storage can fit a member pointer, make sure it's aligned accordingly
	using align_type = ::std::conditional_t<real_size >= sizeof(pointer_types_member), pointer_types_member, pointer_types>;
	union [[gnu::may_alias]] alignas(align_type) data_u {
		::std::byte mem[real_size];
		void* ptr;
	};
	data_u data;

	callable_u() noexcept {
		#ifndef NDEBUG
		data.ptr = nullptr;
		#endif
	}

	template<class T> // requires(!::std::is_pointer_v<T> || ::std::is_function_v<T>)
	callable_u(T&& t) noexcept(noexcept(::std::decay_t<T>{ static_cast<T&&>(t) })) {
		::new (data.mem) ::std::decay_t<T>(static_cast<T&&>(t));
	}
	callable_u(void* t) noexcept : data{ .ptr = t } {}

	callable_u(uninitialized_t) noexcept {}

	callable_u(callable_u&& other) noexcept : data{ .ptr = other.data.ptr } {}
	callable_u& operator=(callable_u&& other) noexcept { ::memcpy(&data, &other.data, sizeof(void*)); return *this; };

	callable_u(const callable_u&) = delete;
	callable_u& operator=(const callable_u&) = delete;

	~callable_u() noexcept {};

	// [[gnu::always_inline]] all these trivial functions to potentially boost debug performance without optimizations
	template<class T> [[gnu::always_inline]]       T& ref()       noexcept { return  *reinterpret_cast<      T*>(data.mem); }
	template<class T> [[gnu::always_inline]] const T& ref() const noexcept { return  *reinterpret_cast<const T*>(data.mem); }

	template<class T, class ... Args>
	void construct(Args&& ... args) noexcept(noexcept(T{ static_cast<Args&&>(args)... })) {
		::new (data.mem) T{ static_cast<Args&&>(args)... };
	}

	template<class T>
	void destroy() noexcept(noexcept(::std::declval<T>().~T())) {
		reinterpret_cast<T*>(data.mem)->~T();
		// Reset pointer, so that holds_pointer won't return true with empty object
		// no need to zero rest of the bytes
		#ifndef NDEBUG
		data.ptr = nullptr;
		#endif
	}

	#ifndef NDEBUG
	bool holds_pointer() const noexcept { return data.ptr != nullptr; }
	#endif
};

} // namespace jpl::detail


#endif // JPL_BITS_CALLABLE_HPP