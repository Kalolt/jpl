/*
	TODO:
		-make nesting optional
		-make zip's loader return a view to mmap in a way that can't dangle (currently it memcpys to new buffer)
	Possible new features:
		-possibly add support for more compression methods for zip
		-custom implementations for other archive types (maybe eventually drop libarchive dependency)		
*/

#include <fcntl.h>
#include <jpl/archive.hpp>
#include <jpl/file_io.hpp>
#include <jpl/bits/err.hpp>

#include <archive.h>
#include <archive_entry.h>
#include <libdeflate.h>
#include <dirent.h>
#include <frozen/set.h>

#include <stdexcept>
#include <string>
#include <memory>
#include <filesystem>

namespace jpl {

static constexpr auto archive_extensions = ::frozen::make_set<::jpl::file_ext>({
	::jpl::file_ext::zip,
	::jpl::file_ext::rar,
	::jpl::file_ext::zip7
});

bool archive::supports(const ::jpl::file_ext ext) {
	return archive_extensions.count(ext);
}

struct archive::libarchive final : public archive::impl {
	::archive* a;
	::archive_entry* entry{ nullptr };
	::std::shared_ptr<::jpl::file_data> source;
	::std::unique_ptr<::jpl::archive::impl> nested;

	libarchive(const char* path)	
		: a{ ::archive_read_new() }
		, source( ::std::make_shared<::jpl::file_data>( jpl::mmap(path) ) )
	{
		::archive_read_support_filter_all(a);
		::archive_read_support_format_all(a);
		int res = ::archive_read_open_memory(a, source->data(), source->size());
		if (res != ARCHIVE_OK) ::jpl::err::archive("archive_read_open_memory", ::archive_error_string(a));
		res = ::archive_read_next_header(a, &entry);
		if (res != ARCHIVE_OK)
			entry = nullptr;
	}

	libarchive(::std::shared_ptr<::jpl::file_data> source)
		: a{ ::archive_read_new() }
		, source( source )
	{
		::archive_read_support_filter_all(a);
		::archive_read_support_format_all(a);
		int res = ::archive_read_open_memory(a, source->data(), source->size());
		if (res != ARCHIVE_OK)
			::jpl::err::archive("archive_read_open_memory", ::archive_error_string(a));
		res = ::archive_read_next_header(a, &entry);
		if (res != ARCHIVE_OK)
			entry = nullptr;
	}

	~libarchive() override {
		if (a) ::archive_read_free(a);
	}

	void next() override;

	bool done() const override {
		return entry == nullptr;
	}
	
	::std::string_view name() const override {
		if (nested) return nested->name();
		assert(::archive_entry_pathname(entry));
		if (nested)
			return nested->name();
		else if (::archive_entry_pathname(entry))
			return ::archive_entry_pathname(entry);
		else
			return "NULL";
	}

	::jpl::data_loader loader() const override {
		if (nested) return nested->loader();
		struct loader_impl {
			std::shared_ptr<::jpl::file_data> source;
			std::string name;
			::jpl::file_data operator()() {
				::archive* a = ::archive_read_new();
				::archive_read_support_filter_all(a);
				::archive_read_support_format_all(a);
				if (::archive_read_open_memory(a, source->data(), source->size()) != ARCHIVE_OK)
					::jpl::err::archive("archive_read_open_memory", ::archive_error_string(a));
				::archive_entry* entry{ nullptr };
				while (::archive_read_next_header(a, &entry) == ARCHIVE_OK) {
					assert(::archive_entry_pathname(entry));
					if (name == ::archive_entry_pathname(entry)) {
						::int64_t size{ ::archive_entry_size(entry) };
						::jpl::file_data buffer{ new char[size], static_cast<::size_t>(size), ::jpl::file_data::type::alloc };
						::ssize_t bytes_read{ ::archive_read_data(a, static_cast<void*>(buffer.data_), size) };
						if (bytes_read < 0) ::jpl::err::archive("archive_read_data", ::archive_error_string(a));
						return buffer;
					}
				}
				throw ::std::runtime_error("file not in archive");
			}
		};
		::std::string_view name_ = name();
		return loader_impl{ source, std::string(name_.begin(), name_.size()) };
	}

	::jpl::file_data data() const override {
		if (nested) return nested->data();
		::int64_t size{ ::archive_entry_size(entry) };
		void* data = ::malloc(size);
		assert(data);
		[[maybe_unused]] ::ssize_t bytes_read{ ::archive_read_data(a, data, size) };
		assert(bytes_read == size);
		return { data, static_cast<::size_t>(size), ::jpl::file_data::type::alloc };
	}
};

struct archive::zip_impl final : public archive::impl {
	::std::shared_ptr<::jpl::file_data> source;
	const char* ptr;
	::std::unique_ptr<::jpl::archive::impl> nested;

	zip_impl(const char* path)
		: source{ std::make_shared<::jpl::file_data>( jpl::mmap(path) ) }
	{
		assert(source->size() > 100);
		const char* eocd = source->end() - 22;
		const char* begin = source->begin();
		while (eocd != begin) {
			::uint32_t sig; ::memcpy(&sig, eocd, 4);
			if (sig == 0x06054b50) {
				::uint16_t comment_len; ::memcpy(&comment_len, eocd + 20, 2);
				if ((eocd + 22 + comment_len) == source->end())
					break;
			}
			eocd--;
		}

		::uint32_t central_dir_size;   ::memcpy(&central_dir_size  , eocd + 12, 4);
		::uint32_t central_dir_offset; ::memcpy(&central_dir_offset, eocd + 16, 4);

		source->advise(central_dir_offset, central_dir_size);

		ptr = source->begin() + central_dir_offset;
		if (empty()) next();
	}

	~zip_impl() override = default;

	void next() override {
		if (nested) {
			nested->next();
			if (!nested->done())
				return;
			nested.reset(nullptr);
		}

		do {
			::uint16_t name_len;    ::memcpy(&name_len   , ptr + 28, 2);
			::uint16_t extra_len;   ::memcpy(&extra_len  , ptr + 30, 2);
			::uint16_t comment_len; ::memcpy(&comment_len, ptr + 32, 2);
			ptr += (46 + name_len + extra_len + comment_len);
		} while (!done() && empty()); // skip over empty entires (directories)

		if (!done()) {
			::jpl::file_ext ext = ::jpl::get_ext(name());
			switch (ext) {
				case ::jpl::file_ext::zip:
					nested.reset(new archive::zip_impl  { data() }); break;
				case ::jpl::file_ext::rar : [[fallthrough]];
				case ::jpl::file_ext::zip7:
					nested.reset(new archive::libarchive{ data() }); break;
				default: break;
			}
		}
	}

	bool done() const override {
		::uint32_t sig; ::memcpy(&sig, ptr, 4);
		return sig != 0x02014b50;
	}

	struct load_info {
		uint32_t offset;
		uint32_t size;
		uint16_t compression;
		uint32_t size_out;
	};

	::jpl::file_data data() const override {
		if (nested) return nested->data();
		load_info info = get_load_info();
		return { source->data_ + info.offset, info.size, ::jpl::file_data::type::view };
	}
	::std::string_view name() const override {
		if (nested) return nested->name();
		::uint16_t name_len; ::memcpy(&name_len, ptr + 28, 2);
		return { ptr + 46, name_len };
	}
	::jpl::data_loader loader() const override {
		if (nested) return nested->loader();
		struct loader_impl {
			std::shared_ptr<jpl::file_data> source;
			load_info info;

			jpl::file_data operator()() {
				source->advise(info.offset, info.size);
				switch (info.compression) {
					case 0: {
						// This could return view to mmap, if there was a way to ensure that it doesn't dangle
						unsigned char* out = static_cast<unsigned char*>(::malloc(info.size));
						::memcpy(out, source->data_ + info.offset, info.size);
						return { out, info.size, ::jpl::file_data::type::alloc };
					} break;
					case 8: {
						unsigned char* out = static_cast<unsigned char*>(::malloc(info.size_out));
						assert(out);
						::libdeflate_decompressor* decompressor = ::libdeflate_alloc_decompressor();
						::libdeflate_deflate_decompress(decompressor, source->data() + info.offset, info.size, out, info.size_out, nullptr);
						::libdeflate_free_decompressor(decompressor);
						return { out, info.size_out, ::jpl::file_data::type::alloc };
					} break;
					default:
						throw std::runtime_error("unsupported compression");
				}
			}
		};
		return loader_impl{ source, get_load_info() };
	}

	private:
	load_info get_load_info() const {
		load_info info;       ::memcpy(&info.offset, ptr + 42, 4);
		::uint16_t name_len;  ::memcpy(&name_len   , source->begin() + info.offset + 26, 2);
		::uint16_t extra_len; ::memcpy(&extra_len  , source->begin() + info.offset + 28, 2);
		info.offset += 30 + name_len + extra_len;

		::memcpy(&info.compression, ptr + 10, 2);
		::memcpy(&info.size       , ptr + 20, 4);
		::memcpy(&info.size_out   , ptr + 24, 4);

		return info;
	}
	bool empty() const noexcept {
		::uint32_t size;
		::memcpy(&size, ptr + 20, 4);
		return size == 0;
	}
};

void archive::libarchive::next() {
	if (nested) {
		nested->next();
		if (!nested->done())
			return;
		nested.reset(nullptr);
	}

	if (::archive_read_next_header(a, &entry) != ARCHIVE_OK) {
		entry = nullptr;
	} else {
		::jpl::file_ext ext = ::jpl::get_ext(name());
		switch (ext) {
			case ::jpl::file_ext::zip:
				nested.reset(new archive::zip_impl  { data() }); break;
			case ::jpl::file_ext::rar : [[fallthrough]];
			case ::jpl::file_ext::zip7:
				nested.reset(new archive::libarchive{ data() }); break;
			default: break;
		}
	}
}

#ifdef __linux__
struct archive::dir_impl final : public archive::impl {
	int fd;
	char buffer[4096];
	::off_t off = 0;
	char* start;
	char* end;
	::std::unique_ptr<::jpl::archive::impl> nested;

	dir_impl(const char* path) : fd{ ::open(path, O_RDONLY) } {
		// if (!fd)
		::ssize_t n_read = ::getdirentries(fd, buffer, 4096, &off);
		start = buffer;
		end = buffer + n_read;
	}
	~dir_impl() override {
		::close(fd);
	};
	void next() override {
		//if (nested) {
		//	nested->next();
		//	if (!nested->done())
		//		return;
		//	nested.reset(nullptr);
		//}
		start += reinterpret_cast<dirent*>(start)->d_reclen;
		if (start == end) {
			::ssize_t n_read = ::getdirentries(fd, buffer, 4096, &off);
			start = buffer;
			end = buffer + n_read;
		}
	}
	bool done() const override { return start == end; };
	::jpl::file_data data() const override {
		//if (nested) return nested->data();
		return ::jpl::mmap(reinterpret_cast<dirent*>(start)->d_name);
	};
	::std::string_view name() const override {
		//if (nested) return nested->name();
		return reinterpret_cast<dirent*>(start)->d_name;
	};
	::jpl::data_loader loader() const override {
		//if (nested) return nested->loader();
		return [path = std::string{ reinterpret_cast<dirent*>(start)->d_name }]{ return ::jpl::mmap(path.c_str()); };
	};
};
#else
struct archive::dir_impl final : public archive::impl {
	std::filesystem::directory_iterator it;
	::std::unique_ptr<::jpl::archive::impl> nested;

	dir_impl(const char* path) : it{ path } {}
	~dir_impl() override = default;
	void next() override {
		if (nested) {
			nested->next();
			if (!nested->done())
				return;
			nested.reset(nullptr);
		}
		it++;
		/*
		if (it != ::std::filesystem::directory_iterator{}) {
			::jpl::file_ext ext = get_ext(it->path().c_str());
			if (archive::supports(ext)) {
				switch (ext) {
					case jpl::file_ext::rar:  [[fallthrough]];
					case jpl::file_ext::zip7:
						nested.reset(new archive::libarchive{ it->path().c_str() }); break;
					case jpl::file_ext::zip:
						nested.reset(new archive::zip_impl  { it->path().c_str() }); break;
					default: break;
				}
			}
		}
		*/
	}
	bool done() const override { return it == ::std::filesystem::directory_iterator{}; };
	::jpl::file_data data() const override {
		if (nested) return nested->data();
		return ::jpl::mmap(it->path().c_str());
	};
	::std::string_view name() const override {
		if (nested) return nested->name();
		return it->path().c_str();
	};
	::jpl::data_loader loader() const override {
		if (nested) return nested->loader();
		return [path = it->path().string()]{ return ::jpl::mmap(path.c_str()); };
	};
};
#endif

archive::iterator::iterator(const char* path)
	: path{ path }
	, impl{ [path]() -> archive::impl* {
		assert(path);
		jpl::file_ext ext{ jpl::get_ext(path) };
		switch (ext) {
			case jpl::file_ext::rar:
			case jpl::file_ext::zip7:
				return new libarchive{ path };
			case jpl::file_ext::zip:
				return new zip_impl{ path };
			default: break;
		}
		if (std::filesystem::is_directory(path))
			return new dir_impl{ path };
		throw std::runtime_error("unsupported path");
	}() }
{}

} // namespace jpl
