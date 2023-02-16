#pragma once

#include <stddef.h>
#include <assert.h>

#include <type_traits>
#include <initializer_list>
#include <limits>

namespace gef {

template<typename T>
struct Slice {
	Slice() = default;
	Slice(T *ptr, size_t len) : ptr(ptr), len(len) {}
	Slice(T *ptr, T *endptr) : ptr(ptr), len(enptr - ptr) {}
	Slice(std::initializer_list<T> list) : ptr(list.begin()), len(list.size()) {}

	T *data() { return ptr; }
	const T *data() const { return ptr; }
	size_t size() const { return len; }

	Slice<T> sub(size_t start, size_t end = std::numeric_limits<size_t>::max()) const {
		if (end > len) {
			end = len;
		}
		if (start > end) {
			start = end;
		}
		return Slice(ptr + start, end - start);
	}

	T &operator[](size_t i) {
		assert(i < len && "index out of bounds");
		return ptr[i];
	}
	
	const T &operator[](size_t i) const {
		assert(i < len && "index out of bounds");
		return ptr[i];
	}

	T *begin() { return ptr; }
	const T *begin() const { return ptr; }
	T *end() { return ptr ? ptr + size : nullptr; }
	const T *end() const { return ptr ? ptr + size : nullptr; }

	T *ptr = nullptr;
	size_t len = 0;
};

} // namespace gef