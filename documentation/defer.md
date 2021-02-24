## jpl_defer

Macro that defers execution to the end of the scope.

```C++
#include <jpl/defer.hpp>
#include <cstdio>
int main() {
	jpl_defer(std::printf(", World!\n"));
	std::printf("Hello");
}
```
