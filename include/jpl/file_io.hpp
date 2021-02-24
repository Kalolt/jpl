#ifndef JPL_FILE_IO_HPP
#define JPL_FILE_IO_HPP

#include <jpl/vector.hpp>
#include <jpl/defer.hpp>
#include <jpl/bits/file_data.hpp>
#include <jpl/bits/err.hpp>

#ifdef __linux__
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#endif

#include <stdexcept>
#include <cstdio>
#include <string>
#include <string_view>

namespace jpl {

struct file_handle {
	::std::FILE* fd;
	::size_t size;
	public:
	file_handle(const char* path)
		: fd{ ::fopen(path, "rb") }
	{
		if (!fd) [[unlikely]] err::open(path);
		::std::fseek(fd, 0, SEEK_END);
		size = ::std::ftell(fd);
		::std::fseek(fd, 0, SEEK_SET);
	}
	~file_handle() {
		::fclose(fd);
	}
	::size_t read(void* buffer) {
		return ::fread(buffer, 1, size, fd);
	}
};

template<class T = ::jpl::vector<::std::byte>>
inline T read_file(const char* path) {
	file_handle file{ path };
	T buffer(file.size);
	[[maybe_unused]] ::size_t n_read = file.read(buffer.data());
	assert(n_read == file.size);
	return buffer;
}

template<class T = ::jpl::vector<::std::byte>>
inline T read_file(const char* path, uint32_t size, uint32_t offset = 0) {
	::std::FILE* file{ ::fopen(path, "rb") };
	if (!file) [[unlikely]] err::open(path);
	jpl_defer( ::fclose(file); );
	
	#ifndef NDEBUG
	::std::fseek(file, 0, SEEK_END);
	assert(size + offset <= ::std::ftell(file));
	::std::fseek(file, offset, SEEK_SET);
	#endif

	T buffer(size);
	[[maybe_unused]] ::size_t n_read = ::std::fread(buffer.data(), 1, size, file);
	assert(n_read == size);

	return buffer;
}

inline ::jpl::file_data mmap(const char* path) {
	int file = ::open(path, O_RDONLY);
	if (file == -1) [[unlikely]] err::open(path);
	struct ::stat sb;
	::fstat(file, &sb);
	char* ptr = static_cast<char*>(::mmap(nullptr, sb.st_size, PROT_READ, MAP_SHARED, file, 0));
	::close(file);
	return { ptr, static_cast<::size_t>(sb.st_size), ::jpl::file_data::type::mmap };
}

class file_lines {
	int fd;

	public:
	file_lines(const char* path)
		: fd{ ::open(path, O_RDONLY) }
	{
		if (fd == -1) [[unlikely]] err::open(path);
	}
	~file_lines() {
		::close(fd);
	}

	class iterator {
		char* mmap;
		const char* end;
		const char* ptr;
		const char* next;

		public:
		iterator(int fd) {
			struct ::stat sb;
			::fstat(fd, &sb);
			mmap = static_cast<char*>(::mmap(nullptr, sb.st_size, PROT_READ, MAP_SHARED, fd, 0));
			if (!mmap) [[unlikely]] err::mmap();
			::madvise(mmap, sb.st_size, MADV_SEQUENTIAL);
			end = mmap + sb.st_size;
			ptr = mmap;
			next = static_cast<const char*>(::memchr(ptr + 1, '\n', end - ptr - 1));
			next = next ? next : nullptr;
		}

		~iterator() {
			::munmap(mmap, end - mmap);
		}

		::std::string_view operator*() {
			return { ptr, static_cast<::size_t>(next - ptr) };
		}

		iterator& operator++() {
			if (next != end) {
				ptr = next + 1;
				next = (const char*)::memchr(ptr, '\n', end - ptr);
				next = next ? next : end;
			} else {
				ptr = end;
			}
			return *this;
		}

		struct end_t {};
		bool operator!=(end_t) const noexcept {
			return ptr != end;
		}
	};

	iterator begin() const noexcept {
		return iterator{ fd };
	}

	iterator::end_t end() const noexcept {
		return {};
	}
};

} // namespace jpl

#endif // JPL_FILE_IO_HPP
