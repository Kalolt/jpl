// Do not consider anything in this file cryptographically secure!

#ifndef JPL_RANDOM_HPP
#define JPL_RANDOM_HPP

#include <stdint.h>
#include <immintrin.h>

namespace jpl {

// Try to generate true random seed with RDSEED. If that fails 1000 times, fall back to RDTSC timestamp.
// https://software.intel.com/content/www/us/en/develop/articles/intel-digital-random-number-generator-drng-software-implementation-guide.html
inline ::uint64_t gen_seed() noexcept {
	unsigned long long seed;
	for (int i = 0; i != 1'000; ++i) {
		if (::_rdseed64_step(&seed)) [[likely]]
			return seed;
		::_mm_pause();
	}
	return ::__rdtsc();
}

// This is equivalent to pcg32 from https://github.com/imneme/pcg-cpp without all the extra compile time bloat,
// and with the addition of custom seed generator
class pcg32 {
	::uint64_t state;
	static constexpr ::uint64_t inc { 1442695040888963407ull };
	static constexpr ::uint64_t mult{ 6364136223846793005ull };

	static constexpr ::uint32_t rotr(::uint32_t i, ::uint32_t n) noexcept {
		return (i >> (n % 32)) | (i << (32 - (n % 32)));
	}

	public:
	constexpr pcg32(const ::uint64_t seed = gen_seed()) : state{ (seed + inc) * mult + inc } {}

	using result_type = ::uint32_t;
	static constexpr ::uint32_t min() noexcept { return 0u; }
	static constexpr ::uint32_t max() noexcept { return UINT32_MAX; }

	::uint32_t operator()() noexcept {
		::uint64_t old_state = state;
		state = state * mult + inc;
		old_state ^= old_state >> 18;
		::uint32_t result = old_state >> 27;
		return rotr(result, old_state >> 59);
	}

	void seed(const ::uint64_t seed) noexcept {
		state = (seed + inc) * mult + inc;
	}
};

// From Squirrel Eiserloh's GDC 2017 talk "Math for Game Programmers: Noise-Based RNG"
inline constexpr ::uint32_t squirrel3(const ::uint32_t position, const ::uint32_t seed = 0) noexcept {
	::uint32_t mangled = position;
	mangled *= 0xB5297A4Du;
	mangled += seed;
	mangled ^= (mangled >> 8);
	mangled += 0x68E31DA4u;
	mangled ^= (mangled << 8);
	mangled *= 0x1B56C4E9u;
	mangled ^= (mangled >> 8);
	return mangled;
}

inline constexpr ::uint32_t noise2d(::uint32_t x, ::uint32_t y, ::uint32_t seed = 0) noexcept {
	return squirrel3(x + 198491317u * y, seed);
}

} // namespace jpl

#endif // JPL_RANDOM_HPP
