#ifndef JPL_FUNCTION_HPP
#define JPL_FUNCTION_HPP

#include <cstddef>
#include <new>
#include <type_traits>
#include <assert.h>
#include <string.h>

#include <jpl/bits/invoke.hpp>

namespace jpl {

struct function_conf {
	::size_t local_storage_size     = 8;
	::size_t max_dummy_size         = 256;
	bool optimize_function_pointers = true;
};

namespace detail {

inline constexpr function_conf function_conf_default{};

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

	template<class T> requires(!::std::is_pointer_v<T>)
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

// If function doesn't deal with complex types, invoker and manager can be merged
// Complex types would cause issues with this design, because the manager has to pass
// and return dummy values matching the function signature.
template<::size_t max_dummy_size, class R, class ... Args>
inline constexpr bool merge_invoker = [](){
	constexpr bool trivial = (::std::is_void_v<R> || ::std::is_trivial_v<R>) && (::std::is_trivial_v<Args> && ...);

	constexpr ::size_t R_size = []{
		if constexpr (!::std::is_void_v<R>)
			return sizeof(R);
		else
			return 0;
	}();

	if constexpr (sizeof...(Args) > 0)
		return trivial && (R_size + (sizeof(Args) + ...) <= max_dummy_size);
	else
		return trivial && (R_size <= max_dummy_size);
}();

// Local storage can be used only if alignment and size fit, and the type is movable
template<class T, class F>
inline constexpr bool use_local_storage = (alignof(typename F::callable_t) % alignof(::std::decay_t<T>) == 0)
                                   && (sizeof(::std::decay_t<T>) <= F::ss)
                                   && (::std::is_move_constructible_v<::std::decay_t<T>>);
// Don't try to store object inside itself!
template<class T, class F> requires ::std::is_same_v<::std::decay_t<T>, F>
inline constexpr bool use_local_storage<T, F> = false;

// https://www.youtube.com/watch?v=e8SyxB3_mnw
template<class T>
using forward_nontrivial = ::std::conditional_t<::std::is_trivially_copyable_v<T>, T, T&&>;

} // namespace detail

template<class, function_conf conf = detail::function_conf_default> class function;

template<class R, class ... Args, function_conf conf>
	requires(!detail::merge_invoker<conf.max_dummy_size, R, Args...>)
class function<R(Args...), conf> {
	public:
	enum class action : char {
		dtor,
		move, // also destroys source!
	};

	static constexpr ::size_t ss = conf.local_storage_size;

	using callable_t = detail::callable_u<conf.local_storage_size>;
	using func_t     = R(*)(Args...);
	using invoker_t  = R(*)(detail::forward_nontrivial<Args>..., callable_t*);
	using manager_t  = void(*)(void*, action);

	struct move_t {
		callable_t* src;
		callable_t* dst;
	};

	callable_t callable{};
	invoker_t  invoker {};
	manager_t  manager {};

	public:
	function() noexcept {}
	function(::std::nullptr_t) noexcept {}

	function(func_t t) noexcept requires(conf.optimize_function_pointers)
		: callable{ t }
	{}

	template<class T> requires (detail::use_local_storage<T, function> && ::std::is_invocable_r_v<R, T, Args...>)
	function(T&& t)
		: callable{ static_cast<T&&>(t) }
		, invoker{ invoke<::std::decay_t<T>> }
		, manager( manage<::std::decay_t<T>> )
	{}

	template<class T>
	function(T&& t) requires (!detail::use_local_storage<T, function> && ::std::is_invocable_r_v<R, T, Args...>)
		: callable{ new ::std::decay_t<T>(static_cast<T&&>(t)) }
		, invoker{ invoke<::std::decay_t<T>> }
		, manager{ manage<::std::decay_t<T>> }
	{}

	~function() { if (manager) manager(&callable, action::dtor); }

	function(const function& other) = delete;
	function& operator=(const function& other) = delete;

	function(function&& other) noexcept {
		swap(other);
	}

	function& operator=(function&& other) noexcept {
		swap(other);
		return *this;
	}

	void swap(function& other) noexcept {
		callable_t temp{ callable_t::uninitialized };
		move_t move;
		if (manager) {
			move = { &callable, &temp };
			manager(&move, action::move);
		} else if constexpr (conf.optimize_function_pointers) {
			temp = static_cast<callable_t&&>(callable);
		}
		if (other.manager) {
			move = { &other.callable, &callable };
			other.manager(&move, action::move);
		} else if constexpr (conf.optimize_function_pointers) {
			callable = static_cast<callable_t&&>(other.callable);
		}
		if (manager) {
			move = { &temp, &other.callable };
			manager(&move, action::move);
		} else if constexpr (conf.optimize_function_pointers) {
			other.callable = static_cast<callable_t&&>(temp);
		}
		::std::swap(manager, other.manager);
		::std::swap(invoker, other.invoker);
	}

	template<class ... RealArgs>
	R operator()(RealArgs&& ... args) const {
		if constexpr (conf.optimize_function_pointers) {
			if (invoker)
				return invoker(static_cast<detail::forward_nontrivial<Args>>(args)..., const_cast<callable_t*>(&callable));
			assert(callable.holds_pointer());
			return callable.template ref<func_t>()(static_cast<Args&&>(args)...);
		} else {
			assert(invoker);
			return invoker(static_cast<detail::forward_nontrivial<Args>>(args)..., const_cast<callable_t*>(&callable));
		}
	}

	private:
	template<class T> requires (detail::use_local_storage<T, function>)
	static R invoke(detail::forward_nontrivial<Args> ... args, callable_t* r) {
		return ::jpl::invoke(r->template ref<::std::decay_t<T>>(), static_cast<Args&&>(args)...);
	}

	template<class T> requires (!detail::use_local_storage<T, function>)
	static R invoke(detail::forward_nontrivial<Args> ... args, callable_t* r) {
		return ::jpl::invoke(*static_cast<T*>(r->data.ptr), static_cast<Args&&>(args)...);
	}

	template<class T> requires(detail::use_local_storage<T, function>)
	static void manage(void* ptr, action act) {
		switch (act) {
			case action::dtor: {
				if constexpr (!::std::is_trivially_destructible_v<T>) {
					callable_t* r = static_cast<callable_t*>(ptr);
					r->template destroy<T>();
				}
			} break;
			case action::move: {
				auto [src, dst] = *static_cast<move_t*>(ptr);
				dst->template construct<T>(static_cast<T&&>(src->template ref<T>()));
				src->template destroy<T>();
			} break;
		}
	}
	
	template<class T> requires(!detail::use_local_storage<T, function>)
	static void manage(void* ptr, action act) {
		switch (act) {
			case action::dtor: {
				callable_t* r = static_cast<callable_t*>(ptr);
				delete static_cast<T*>(r->data.ptr);
			} break;
			case action::move: {
				auto [src, dst] = *static_cast<move_t*>(ptr);
				dst->data.ptr = src->data.ptr;
			} break;
		}
	}
};

// This version doesn't need to support member functions, because member functions 
// always take a reference to the object, and references aren't trivial
template<class R, class ... Args, function_conf conf>
	requires (detail::merge_invoker<conf.max_dummy_size, R, Args...>)
class function<R(Args...), conf> {
	public:
	enum class action : char {
		call,
		dtor,
		move, // also destroys source!
	};

	static constexpr ::size_t ss = conf.local_storage_size;

	using callable_t = detail::callable_u<conf.local_storage_size>;
	using func_t = R(*)(Args...);
	using invoker_t = R(*)(detail::forward_nontrivial<Args>..., void*, action);

	struct move_t {
		callable_t* src;
		callable_t* dst;
	};

	callable_t callable{};
	invoker_t  invoker {};

	public:
	function() noexcept {}
	function(::std::nullptr_t) noexcept {}

	function(func_t t) noexcept requires(conf.optimize_function_pointers)
		: callable{ t }
	{}

	template<class T> requires (detail::use_local_storage<T, function> && ::std::is_invocable_r_v<R, T, Args...>)
	function(T&& t)
		: callable{ static_cast<T&&>(t) }
		, invoker{ invoke<::std::decay_t<T>> }
	{}

	template<class T> requires (!detail::use_local_storage<T, function> && ::std::is_invocable_r_v<R, T, Args...>)
	function(T&& t)
		: callable{ new ::std::decay_t<T>{ static_cast<T&&>(t) } }
		, invoker{ invoke<::std::decay_t<T>> }
	{}

	~function() noexcept { if (invoker) invoker(Args{}..., &callable, action::dtor); }

	function(const function& other) = delete;
	function& operator=(const function& other) = delete;

	function(function&& other) noexcept {
		swap(other);
	}

	function& operator=(function&& other) noexcept {
		swap(other);
		return *this;
	}

	// Assign callable
	template<class T>
	function& operator=(T&& other) {
		if (invoker) {
			invoker(Args{}..., &callable, action::dtor);
			invoker = nullptr;
		}
		if constexpr (conf.optimize_function_pointers && ::std::is_convertible_v<T, func_t>) {
			callable = other;
		} else if constexpr (detail::use_local_storage<T, function>) {
			invoker = invoke<::std::decay_t<T>>;
			callable.template construct<::std::decay_t<T>>(static_cast<T&&>(other));
		} else {
			invoker = invoke<::std::decay_t<T>>;
			callable = new ::std::decay_t<T>(static_cast<T&&>(other));
		}
		return *this;
	}

	void swap(function& other) noexcept {
		callable_t temp{ callable_t::uninitialized };
		move_t move;
		if (invoker) {
			move = { &callable, &temp };
			invoker(Args{}..., &move, action::move);
		} else if constexpr (conf.optimize_function_pointers) {
			temp = static_cast<callable_t&&>(callable);
		}
		if (other.invoker) {
			move = { &other.callable, &callable };
			other.invoker(Args{}..., &move, action::move);
		} else if constexpr (conf.optimize_function_pointers) {
			callable = static_cast<callable_t&&>(other.callable);
		}
		if (invoker) {
			move = { &temp, &other.callable };
			invoker(Args{}..., &move, action::move);
		} else if constexpr (conf.optimize_function_pointers) {
			other.callable = static_cast<callable_t&&>(temp);
		}
		::std::swap(invoker, other.invoker);
	}

	template<class ... RealArgs>
	R operator()(RealArgs&& ... args) const {
		if constexpr (conf.optimize_function_pointers) {
			if (invoker)
				return invoker(static_cast<detail::forward_nontrivial<Args>>(args)..., const_cast<callable_t*>(&callable), action::call);
			assert(callable.holds_pointer());
			return (callable.template ref<func_t>())(static_cast<detail::forward_nontrivial<Args>>(args)...);
		} else {
			assert(invoker);
			return invoker(static_cast<detail::forward_nontrivial<Args>>(args)..., const_cast<callable_t*>(&callable), action::call);
		}
	}

	private:

	template<class T> requires(detail::use_local_storage<T, function>)
	static R invoke(detail::forward_nontrivial<Args> ... args, void* ptr, const action act) {
		switch (act) {
			case action::call: {
				callable_t* r = static_cast<callable_t*>(ptr);
				return (r->template ref<T>())(static_cast<detail::forward_nontrivial<Args>>(args)...);
			} break;
			case action::dtor: {
				if constexpr (!::std::is_trivially_destructible_v<T>) {
					callable_t* r = static_cast<callable_t*>(ptr);
					r->template destroy<T>();
				}
			} break;
			case action::move: {
				auto [src, dst] = *static_cast<move_t*>(ptr);
				dst->template construct<T>(static_cast<T&&>(src->template ref<T>()));
				src->template destroy<T>();
			} break;
		}
		if constexpr (!::std::is_void_v<R>)
			return {};
	}

	template<class T> requires(!detail::use_local_storage<T, function>)
	static R invoke(detail::forward_nontrivial<Args> ... args, void* ptr, const action act) {
		switch (act) {
			case action::call: {
				callable_t* r = static_cast<callable_t*>(ptr);
				return (*r->template ref<T*>())(static_cast<detail::forward_nontrivial<Args>>(args)...);
			} break;
			case action::dtor: {
				callable_t* self = static_cast<callable_t*>(ptr);
				delete self->template ref<T*>();
			} break;
			case action::move: {
				auto [src, dst] = *static_cast<move_t*>(ptr);
				dst->template construct<T*>(src->template ref<T*>());
			} break;
		}
		if constexpr (!::std::is_void_v<R>)
			return {};
	}
};

template<class R, class ... Args, function_conf conf>
void swap(function<R(Args...), conf>& lhs, function<R(Args...), conf>&rhs) {
	lhs.swap(rhs);
}

template<class R, class ... Args>
function(R(*)(Args...)) -> function<R(Args...)>;

using procedure = function<void()>;

} // namespace jpl

#endif // JPL_FUNCTION_HPP
