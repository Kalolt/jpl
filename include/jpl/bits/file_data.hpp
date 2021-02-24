#ifndef JPL_FILE_DATA_HPP
#define JPL_FILE_DATA_HPP

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <assert.h>

namespace jpl {

struct file_data {
	enum class type : ::uint8_t {
		view  = 0,
		mmap  = 1,
		alloc = 2,
	};
	enum class err : ::uint8_t {
		success    = 0,
		not_found  = 1,
		access     = 2,
		map_failed = 3,
	};

	char* data_;
	  type     type_ :  2{ type::view };
	  err      err_  :  6{ err::success };
	::uint64_t size_ : 56{ 0 };

	file_data() noexcept {}

	file_data(void* data, ::uint64_t size, type type_) noexcept
		: data_{ static_cast<char*>(data) }, type_{ type_ }, size_{ size }
	{}

	file_data(err err_) noexcept
		: err_{ err_ }
	{}

	file_data(const file_data&) = delete;
	file_data& operator=(const file_data&) = delete;
	file_data(file_data&& other) noexcept : data_{ other.data_ }, type_{ other.type_ }, size_{ other.size_ } {
		other.data_ = nullptr;
		other.type_ = type::view;
	}
	file_data& operator=(file_data&& other) noexcept {
		data_ = other.data_;
		size_ = other.size_;
		type_ = other.type_;
		other.data_ = nullptr;
		other.type_ = type::view;
		return *this;
	}
	~file_data() noexcept {
		switch (type_) {
			case type::mmap:  { ::munmap(data_, size_); } break;
			case type::alloc: { ::free(data_)         ; } break;
			case type::view: break;
		}
	}

	const char*  begin() const noexcept { return data_; }
	const char*  end()   const noexcept { return data_ + size_; }
	    ::size_t size()  const noexcept { return size_; };
	const char*  data()  const noexcept { return data_; };

	const char* err_text() const noexcept {
		switch (err_) {
			case err::access:     return "unable to access file";
			case err::map_failed: return "mmap failed";
			case err::not_found:  return "file not found";
			case err::success:    return "success";
		}
		return "file_data unknown error";
	}

	void advise(::size_t offset, ::size_t size) noexcept {
		int align = offset & 4095; // Align pointer to page size
		[[maybe_unused]] int res = ::madvise(data_ + offset - align, size + align, MADV_SEQUENTIAL);
		assert(res == 0);
	}

	operator const char*() const noexcept {
		return data_;
	}
	operator const unsigned char*() const noexcept {
		return reinterpret_cast<const unsigned char*>(data_);
	}

	operator bool() const noexcept {
		return err_ == err::success;
	}
};

} // namespace jpl

#endif // JPL_FILE_DATA_HPP
