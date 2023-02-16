#pragma once

#include <stddef.h>
#include <assert.h>
#include <type_traits>
#include <system/allocator.h>

// NOTE: it only works with the default g_alloc
// TODO: maybe make it work with any allocator? would probably require more memory (IAllocator *alloc)
namespace gef {
	template<typename T>
	struct ptr {
		ptr() = default;
		ptr(nullptr_t) : buf(nullptr) {}
		ptr(const T *buf) = delete;
		ptr(T *buf) : buf(buf) {}
		ptr(const ptr &other) = delete;
		ptr(ptr &&other) { *this = std::move(other); }
		~ptr() { destroy(); }

		template<typename ...TArgs>
		static ptr make(TArgs &&...args) {
			return g_alloc->make<T>(args...);
		}

		T *get() { return buf; }
		const T *get() const { return buf; }

		T *release() { T *b = buf; buf = nullptr; return b; }

		void destroy() {
			if (!buf) return;
			g_alloc->destroy(buf);
			buf = nullptr;
		}

		ptr &operator=(const ptr &other) = delete;
		ptr &operator=(ptr &&other) { std::swap(buf, other.buf); return *this; }

		operator bool() const { return buf != nullptr; }
		bool operator!() const { return buf == nullptr; }
		T *operator->() { return buf; }
		T &operator*() { assert(buf); return *buf; }
		const T *operator->() const { return buf; }
		const T &operator*() const { assert(buf); return *buf; }

		T *buf = nullptr;
	};

	template<typename T>
	struct ptr<T[]> {
		ptr() = default;
		ptr(T *buf) : buf(buf) {}
		ptr(const ptr &other) = delete;
		ptr(ptr<T> &&other) = delete;
		ptr(ptr &&other) { *this = std::move(other); }
		~ptr() { destroy(); }

		template<typename ...TArgs>
		static ptr make(size_t n, TArgs &&...args) {
			return g_alloc->makeArr<T>(n, args...);
		}

		T *get() { return buf; }
		const T *get() const { return buf; }

		T *release() { T *b = buf; buf = nullptr; return b; }

		void destroy() {
			if (!buf) return;
			g_alloc->destroyArr(buf);
			buf = nullptr;
		}

		ptr &operator=(const ptr &other) = delete;
		ptr &operator=(ptr &&other) { buf = other.buf; other.buf = nullptr; return *this; }

		operator T *() { return buf; }
		template<typename Q>
		operator Q *() { return (Q *)buf; }
		operator bool() const { return buf != nullptr; }
		bool operator!() const { return buf == nullptr; }
		T *operator->() { return buf; }
		T &operator*() { return assert(buf); *buf; }

		T *buf = nullptr;
	};
} // namespace gef
