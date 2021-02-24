#ifndef JPL_ARCHIVE_HPP
#define JPL_ARCHIVE_HPP

#include <jpl/function.hpp>
#include <jpl/file_types.hpp>
#include <jpl/bits/file_data.hpp>

#include <string_view>

namespace jpl {
using data_loader = ::jpl::function<::jpl::file_data()>;

struct archive {
	const char* path;

	static bool supports(const ::jpl::file_ext);

	struct impl {
		virtual void next() = 0;
		virtual bool done() const = 0;
		virtual ::jpl::file_data data() const = 0;
		virtual ::std::string_view name() const = 0;
		virtual ::jpl::data_loader loader() const = 0;
		virtual ~impl() {};
	};
	struct libarchive;
	struct zip_impl;
	struct dir_impl;

	struct iterator {
		const char* path;
		archive::impl* impl;

		iterator(const char* path);
		~iterator() {
			delete impl;
		}

		::jpl::file_data   data  () const { return impl->data()  ; }
		::std::string_view name  () const { return impl->name()  ; }
		::jpl::data_loader loader() const { return impl->loader(); }

		struct result {
			::std::string_view name;
			::jpl::data_loader loader;
		};
		const iterator& operator*() {
			return *this;
		}

		iterator& operator++() {
			impl->next();
			return *this;
		}

		struct end {};
		bool operator!=(end) const noexcept {
			return !impl->done();
		}
	};

	iterator begin() {
		return { path };
	}

	struct iterator_end {};
	iterator::end end() { return {}; };
};

} // namespace jpl

#endif // JPL_ARCHIVE_HPP

#ifdef JPL_HEADER_ONLY
#ifndef JPL_ARCHIVE_IMPL
#define JPL_ARCHIVE_IMPL
#include <jpl/src/archive.cpp>
#endif
#endif
