#include <jpl/file_types.hpp>

#include <frozen/unordered_map.h>

template<>
struct frozen::elsa<::std::string_view> {
	constexpr ::size_t operator()(const ::std::string_view str, ::size_t seed) const noexcept {
		// frozen guarantees perfect hashing, so a simple hash function leads to better
		// run-time performance at the cost of potentially increased compile time.
		::size_t hash{ seed };
		for (const char c : str)
			hash ^= c;
		return hash;
	}
};

namespace jpl {

constexpr auto extensions = ::frozen::make_unordered_map<::std::string_view, file_ext>({
	{ ".png",  file_ext::png  },
	{ ".jpg",  file_ext::jpeg },
	{ ".jpeg", file_ext::jpeg },
	{ ".bmp",  file_ext::bmp  },
	{ ".tga",  file_ext::tga  },
	{ ".gif",  file_ext::gif  },
	{ ".zip",  file_ext::zip  },
	{ ".cbz",  file_ext::zip  },
	{ ".rar",  file_ext::rar  },
	{ ".cbr",  file_ext::rar  },
	{ ".7z" ,  file_ext::zip7 },
});

file_ext get_ext(::std::string_view path) {
	const char* start = static_cast<const char*>(::memrchr(path.data(), '.', path.size()));
	if (!start) return file_ext::unknown;
	std::string ext{ start, path.end() };
	
	// Convert to lower case. This trick has a bunch of caveats, but it's safe here.
	for (auto c : ext)
		c |= 32;

	if (extensions.count(ext))
		return extensions.at(ext);
	return file_ext::unknown;
}

} // namespace jpl
