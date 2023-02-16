#pragma once

#include <assert.h>
#include <initializer_list>
#include <functional>

#include <system/allocator.h>
#include <maths/math_utils.h>

#undef max

/*
functions:

// cleans up the vector by destroying the underlying data
void destroy()
// set the allocator that will be used to allocate the data
void setAllocator(IAllocator *new_allocator)
// reserve space for new allocations
void reserve(size_t n)
// resize the vector to size n, if <n> is smaller than the current size
// it will destroy the items, it it is larget it will create new ones
// using <args>
void resize(size_t n, TArgs &&...args)
// creates an item at the end of the vector
void emplace_back(TArgs &&...args)
// moves <val> to the end of the vector
void push_back(T &&val)
// copies <val> to the end of the vector
void push_back(const T &val)
// erases the item at the position specified by the iterator
void eraseIt(const_iterator iter)
// erases the item at the <index> position
void erase(size_t index)
// erases the item at the position specified by the iterator by swapping it
// with the last object, this does NOT keep the vector's order but it is 
// much faster
void eraseSwapIt(const_iterator iter)
// erases the item at the <index> position  by swapping it
// with the last object, this does NOT keep the vector's order 
// but it is much faster
void eraseSwap(size_t index)
// reallocates the vector to be able to hold at least <newcap> items
void reallocate(size_t newcap)
// destroys all the items in the vector
void clear()
// returns how many items are in the vector
size_t size() const
// returns how many items can be pushed in the vector before needing to reallocate
size_t capacity() const
// returns the data
T *data()
// returns the first item in the vector by reference. this will crash if there are 
// no items
T &front()
// returns the last item in the vector by reference. this will crash if there are 
// no items
T &back()
// returns true if there are no items in the vector
bool empty() const
// finds an item by comparing it, returns SIZE_MAX when nothing is found
size_t find(const T &item)
// gets the index of an iterator
size_t findIt(const_iterator it)
// iterates all the items in the vector and returns when <fn> returns true
T *findWhen(bool (*fn)(T &obj, size_t ind))
// iterates all the items in the vector and returns when <fn> returns true,
// also has a userdata value
T *findWhen(bool (*fn)(T &obj, size_t ind, const Q &udata), const Q &udata)
*/

namespace gef {
	template<typename T>
	struct Vec {
		using iterator = T *;
		using const_iterator = const T *;

		Vec() = default;
		Vec(const Vec &v) { *this = v; }
		Vec(Vec &&v) { *this = std::move(v); }
		Vec(IAllocator *alloc) : allocator(alloc) {}
		Vec(std::initializer_list<T> list) {
			reserve(list.size());
			for (auto &&val : list) {
				emplace_back(std::move(val));
			}
		}

		~Vec() {
			destroy();
		}

		// cleans up the vector by destroying the underlying data
		void destroy() {
			for (size_t i = 0; i < len; ++i) {
				buf[i].~T();
			}
			allocator->dealloc(buf);
			buf = nullptr;
			len = cap = 0;
		}

		// set the allocator that will be used to allocate the data
		void setAllocator(IAllocator *new_allocator) {
			assert(!buf);
			if (new_allocator) {
				allocator = new_allocator;
			}
		}

		// reserve space for new allocations
		void reserve(size_t n) {
			if (cap < n) {
				reallocate(gef::max(cap * 2, n));
			}
		}

		// resize the vector to size n, if <n> is smaller than the current size
		// it will destroy the items, it it is larget it will create new ones
		// using <args>
		template<typename ...TArgs>
		void resize(size_t n, TArgs &&...args) {
			if (n < len) {
				for (size_t i = n; i < len; ++i) {
					buf[i].~T();
				}
			}
			else if (n > len) {
				reserve(n);
				for (size_t i = 0; i < n; ++i) {
					new (buf + i) T(std::move(args)...);
				}
			}
			len = n;
		}

		// creates an item at the end of the vector
		template<typename ...TArgs>
		void emplace_back(TArgs &&...args) {
			reserve(len + 1);
			new (buf + len) T(std::move(args)...);
			len++;
		}

		// moves <val> to the end of the vector
		void push_back(T &&val) {
			reserve(len + 1);
			new (buf + len) T(std::move(val));
			len++;
		}

		// copies <val> to the end of the vector
		void push_back(const T &val) {
			reserve(len + 1);
			new (buf + len) T(val);
			len++;
		}

		// erases the item at the position specified by the iterator
		void eraseIt(const_iterator iter) {
			if (iter < begin() || iter >= end()) {
				return;
			}
			size_t index = (size_t)(iter - buf);
			erase(index);
		}

		// erases the item at the <index> position
		void erase(size_t index) {
			if (index >= len) {
				return;
			}
			len--;
			for (size_t i = index; i < len; ++i) {
				buf[i] = std::move(buf[i + 1]);
			}
		}

		// erases the item at the position specified by the iterator by swapping it
		// with the last object, this does NOT keep the vector's order but it is 
		// much faster
		void eraseSwapIt(const_iterator iter) {
			if (iter < begin() || iter >= end()) {
				return;
			}
			size_t index = (size_t)(iter - buf);
			eraseSwap(index);
		}

		// erases the item at the <index> position  by swapping it
		// with the last object, this does NOT keep the vector's order 
		// but it is much faster
		void eraseSwap(size_t index) {
			if (index >= len) {
				return;
			}
			buf[index] = std::move(buf[--len]);
		}

		// reallocates the vector to be able to hold at least <newcap> items
		void reallocate(size_t newcap) {
			if (newcap < cap) {
				newcap = cap * 2;
			}
			T *newbuf = (T *)allocator->allocDebug(sizeof(T) * newcap, typeid(T).name());
			for (size_t i = 0; i < len; ++i) {
				new (newbuf + i) T(std::move(buf[i]));
			}
			allocator->dealloc(buf);
			buf = newbuf;
			cap = newcap;
		}

		// destroys all the items in the vector
		void clear() {
			for (size_t i = 0; i < len; ++i) {
				buf[i].~T();
			}
			len = 0;
		}

		// returns how many items are in the vector
		size_t size() const {
			return len;
		}

		// returns how many items can be pushed in the vector before needing to reallocate
		size_t capacity() const {
			return cap;
		}

		// returns the data
		T *data() {
			return buf;
		}

		// returns the data
		const T *data() const {
			return buf;
		}

		T &operator[](size_t ind) {
			assert(ind < len);
			return buf[ind];
		}

		const T &operator[](size_t ind) const {
			assert(ind < len);
			return buf[ind];
		}

		T *begin() {
			return buf;
		}

		const T *begin() const {
			return buf;
		}

		T *end() {
			return buf + len;
		}

		const T *end() const {
			return buf + len;
		}

		// returns the first item in the vector by reference. this will crash if there are 
		// no items
		T &front() {
			assert(buf);
			return buf[0];
		}

		// returns the first item in the vector by reference. this will crash if there are 
		// no items
		const T &front() const {
			assert(buf);
			return buf[0];
		}

		// returns the last item in the vector by reference. this will crash if there are 
		// no items
		T &back() {
			assert(len);
			return buf[len - 1];
		}

		// returns the last item in the vector by reference. this will crash if there are 
		// no items
		const T &back() const {
			assert(len);
			return buf[len - 1];
		}

		// returns true if there are no items in the vector
		bool empty() const {
			return len == 0;
		}

		Vec &operator=(const Vec &v) {
			clear();
			reserve(v.len);
			for (size_t i = 0; i < v.len; ++i) {
				emplace_back(v[i]);
			}
			len = v.len;
			return *this;
		}

		Vec &operator=(Vec &&v) {
			std::swap(buf, v.buf);
			std::swap(len, v.len);
			std::swap(cap, v.cap);
			std::swap(allocator, v.allocator);
			return *this;
		}

		// finds an item by comparing it, returns SIZE_MAX when nothing is found
		size_t find(const T &item) const {
			for (size_t i = 0; i < len; ++i) {
				if (buf[i] == item) {
					return i;
				}
			}
			return SIZE_MAX;
		}

		// gets the index of an iterator
		size_t findIt(const_iterator it) const {
			if (it < buf || it >(buf + len)) {
				return SIZE_MAX;
			}
			return (size_t)(it - buf);
		}

		// iterates all the items in the vector and returns when <fn> returns true
		T *findWhen(bool (*fn)(T &obj, size_t ind)) {
			for (size_t i = 0; i < len; ++i) {
				if (fn(buf[i], i)) {
					return buf + i;
				}
			}
			return nullptr;
		}

		// iterates all the items in the vector and returns when <fn> returns true
		const T *findWhen(bool (*fn)(T &obj, size_t ind)) const {
			for (size_t i = 0; i < len; ++i) {
				if (fn(buf[i], i)) {
					return buf + i;
				}
			}
			return nullptr;
		}

		// iterates all the items in the vector and returns when <fn> returns true,
		// also has a userdata value
		template<typename Q>
		T *findWhen(bool (*fn)(T &obj, size_t ind, const Q &udata), const Q &udata) {
			for (size_t i = 0; i < len; ++i) {
				if (fn(buf[i], i, udata)) {
					return buf + i;
				}
			}
			return nullptr;
		}

		// iterates all the items in the vector and returns when <fn> returns true,
		// also has a userdata value
		template<typename Q>
		const T *findWhen(bool (*fn)(T &obj, size_t ind, const Q &udata), const Q &udata) const {
			for (size_t i = 0; i < len; ++i) {
				if (fn(buf[i], i, udata)) {
					return buf + i;
				}
			}
			return nullptr;
		}

	private:
		T *buf = nullptr;
		size_t len = 0;
		size_t cap = 0;
		IAllocator *allocator = g_alloc;
	};
} // namespace gef
