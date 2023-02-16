#pragma once

#include <stdint.h>
#include <stddef.h>
#include <type_traits>

#include <system/allocator.h>

// 10KB minimal node size
constexpr size_t arena_min_size = 1024 * 10; 

struct ArenaNode {
	uint8_t *data;
	size_t size;
	size_t position;
	ArenaNode *next;
	
	static ArenaNode *make(IAllocator *allocator, size_t buffer_size);
	static void release(IAllocator *allocator, ArenaNode *node);
	bool hasSpace(size_t needed_size);
	void *get(size_t object_size);
};

struct Arena : public IAllocator {
	~Arena();

	virtual void *alloc(size_t n) override;
	virtual void dealloc(void *ptr) override;
	virtual void cleanup() override;

	void setInitialSize(size_t size);
	void setAllocator(IAllocator *new_alloc, bool should_own = false);

	template<typename T, typename ...TArgs>
	T *make(TArgs &&...args) {
		T *ptr = (T *)alloc(sizeof(T));
		if (!ptr) return nullptr;
		return new (ptr) T(args...);
	}
	
private:
	void checkAllocator();

	ArenaNode *head = nullptr;
	IAllocator *allocator = nullptr;
	bool owns_allocator = false;
};