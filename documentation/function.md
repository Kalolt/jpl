# jpl::function

A lightweight and fast alternative to std::function.

## Configuration struct

jpl::function is optionally configured by passing a jpl::function_conf configuration struct as the second template argument.

```C++
struct function_conf {
	// Max size of objects stored locally without dynamic allocation (increases size)
	std::size_t local_storage_size  = 8;  
	// Maximum combined size of trivial parameters and return type to consider
	// merging internal invocator and manager functions for smaller object size.
	std::size_t max_dummy_size      = 256;
	// Reduce indirection when calling a stored raw function pointer, at the cost of an added branch
	bool optimize_function_pointers = true;
};
```

## Small buffer optimization (SBO)

jpl::function uses SBO, which can be adjusted by configuring the local_storage_size member of the config struct. Local storage size is the maximum size for which an object can be stored inside the function wrapper, not requiring dynamic allocation.

Increasing the local storage size also increases the size of the object. The total size will be 1 or 2 function pointers (usually 8 or 16 bytes) larger than the local storage size.

```C++
#include <jpl/function.hpp>
jpl::function<void()> func1; // By default local storage size is 8 bytes.
static_assert(sizeof(func1) == 16);
constexpr jpl::function_conf conf{ .local_storage_size = 32 };
jpl::function<void(), conf> func2; // func2 can wrap objects up to 32 bytes in size without dynamic allocation
static_assert(sizeof(func2) == 40);
```

Most std::function implementations have a similar system, but at a fixed size. Usually the size is at least 16 bytes, so that they can fit a member function pointer.

Since I consider member function pointers to be quite a niche thing to wrap, I didn't want to increase the default storage size just to fit them, but this means that if you don't manually set the local storage size to 16 or higher, member function pointers will require dynamic allocation.

Local storage is not used, if the stored object cannot be noexcept moved, or if its alignment requirement is incompatible with the local storage.

## Advantages of using jpl::function over std::function:

### Smaller size

| Wrapper                      | Size   |
|:-----------------------------|-------:|
| std::function (libstdc++)    |     32 |
| std::function (libc++)       |     48 |
| jpl::function                | 16/24* |

\*jpl::function can merge the interal invoker and manager functions, if the return type and all parameter types are trivial. Since this feature creates dummy values when the wrapper is managed, it is disabled when the total size of the required dummy values would be too large.

You can adjust this threshold size through the local_storage_size member of the configuration struct of type jpl::function_conf.

```C++
#include <jpl/function.hpp>
#include <string>
// Function only involves trivial types, so invoker and manager and combined, therefore size is 16
jpl::function<int(const char*, int)> func1;
static_assert(sizeof(func1) == 16);
// std::string is a non-trivial type, so invoker and manager will not be combined
jpl::function<std::string(int)> func2;
static_assert(sizeof(func2) == 24);
// all pointers are trivial
jpl::function<int(std::string*)> func3;
static_assert(sizeof(func3) == 16);
// all references are non-trivial
jpl::function<int(int&)> func4;
static_assert(sizeof(func4) == 24);
```

```C++
#include <jpl/function.hpp>
// Create a static constexpr config struct, which sets dummy size to 0, disabling merging invoker and manager
static constexpr jpl::function_conf conf{ .max_dummy_size = 0 };
jpl::function<int(const char*, int), conf> func;
static_assert(sizeof(func) == 24);
```

### Customizable local storage size.

You can avoid dynamic allocations by customizing the local storage capacity to fit the types you're wrapping.

```C++
#include <jpl/function.hpp>
int main()
{
	int a = 1, b = 2, c = 3, d = 4;
	auto lambda = [&]{ return a + b + c + d; };
	static_assert(sizeof(lambda) == 32); // lambda captures 4 references at 8 bytes each, so 4 * 8 = 32
	jpl::function<int()> func1{ lambda }; // default local storage size is 8, so wrapping the lambda requires dynamic allocation
	static_assert(sizeof(func1) == 16);
	static constexpr jpl::function_conf conf{ .local_storage_size = 32 };
	jpl::function<int(), conf> func2{ lambda }; // This version is able to store the object within the wrapper, so there's no allocation
	static_assert(sizeof(func2) == 40);
}
```

As long as you don't wrap huge objects, it makes sense to configure the local storage size so that it fits the largest type it's going to wrap.

### Faster than std::function in almost all situations.

- If the stored callable is a plain function pointer, jpl::function can avoid a layer of indirection by calling it directly from the operator() rather than calling it through an intermediate invoker function. This exchanges a possible cache miss with a possible branch misprediction, which usually is lesser of the two evils. If you know that you'll never store raw pointers, this optimization can be disabled through the configuration struct.
```C++
// Disable raw function pointer optimization for potentially better performance when no raw function pointers are stored.
#include <jpl/function.hpp>
static constexpr jpl::function_conf conf{ .optimize_function_pointers = false };
jpl::function<void(), conf> func;
```
- Smaller default size makes jpl::function more cache friendly in general.
- When used properly, customizable local storage size can lead to a significant speed increase, because it improves cache coherency and reduces the number of dynamic allocations.
- jpl::function uses the call convention optimization used by for example zoo::function and folly::function, as presented by Eduardo Madrid at CppCon2020: https://youtu.be/e8SyxB3_mnw
