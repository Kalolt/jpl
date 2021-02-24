# jpl::vector

## Why use jpl::vector over std::vector?

jpl::vector has arguably better API with no std::initializer_list initialization, and instead more varied forms of initialization such as initialization from predicate, or initialization from parameter pack (using the jpl::list token as first argument).

jpl::vector is also often faster than std::function for trivial types, because jpl::vector doesn't default-initialize trivial types. jpl::vector also uses an allocator based on the jpl::allocator concept, which supports realloc, which can make resizing of the vector of trivial values significantly more efficient. std::vector is mandated by the standard to use allocator based on Allocator named requirement, which doesn't support realloc, thus it must always allocate a new buffer, move the contents of the old buffer to the new one, and free the old buffer, whenever its capacity changes.

## Known issues:

+ Predicate initialization can be an annoyance, when you want the predicate itself to be the value you initialize with, such as when creating a vector of threads all with the same task. In this case you can wrap the predicate in a lambda.

# differences to std::vector:

## Added:
+ size_bytes() => size() * sizeof(value_type)
+ Constructor with jpl::list token to avoid std::initializer_list. To construct a jpl::vector from a list of values, pass the jpl::list token as the first parameter, and the following parameters are handled similarly to an std::initializer_list, but with the added benefit of enabling move-semantics.
	+ Do note that the overuse of this constructor could lead to code bloat with lots of template instanciations.
+ The reserve method has an overload with "jpl::sic"-token to reserve exact size. This token also skips sanity checks, so make sure that the new capacity is greater than the current size of the vector.
+ Small buffer optimization (SBO). The vector can optionally hold some constexpr number of elements inside the vector object, similarly to std::string's small string optimization. As long as the size/capacity of the vector isn't grown beyond the SBO size, the vector won't need to make any heap allocations.
	+ Do not overuse this feature! If the vector ends up growing beyond its SBO size, it will be left with wasted space. Therefore you should only use this when you are 99% sure the vector won't grow beyond a certain size, but you can't use std::array due to the 1% chance that it needs to be larger.
	+ Remember that each time you create a jpl::vector with different SBO size, you are instanciating a new class.
```C++
#include <jpl/vector.hpp>
int main() {
	// std::initializer_list style initialization with jpl::list token
	jpl::vector<int> vec{ jpl::list, 1, 2, 3 };
	// Just like with std::vector, this makes the capacity at least 5, but it could be higher.
	vec.reserve(5);
	// The jpl::sic token forces an exact value for the new capacity.
	vec.reserve(jpl::sic, 10);
	// create a vector with capacity 10 allocated on heap
	jpl::vector<int> vec2{ jpl::capacity, 10 };
	// create a vector with small buffer optimization (SBO) size 10
	jpl::vector<int, 10> vec3{};
}
```
+ Initialization from a predicate. Predicate can either take no parameter or integral value representing the element's index in the vector.
```C++
#include <jpl/vector.hpp>
int main() {
	jpl::vector<int> vec1{ 5, [](std::size_t i){ return i * i; } }; // { 0, 1, 4, 9, 16 }
	jpl::vector<int> vec2{ 5, [i = 5]() mutable { return i++; } };  // { 5, 6, 7, 8, 9 }
}
```

## Changed:
+ Trivial types (based on std::is_trivial) might not be default initialized at construction.
```C++
#include <jpl/vector.hpp>
jpl::vector<int> vec{ 10 }; // The values in this vector are uninitialized!
```

## Removed:
+ initializer_list constuctor (use jpl::list initialization)
+ max_size