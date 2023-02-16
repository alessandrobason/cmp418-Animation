#pragma once

#include <stdlib.h>
#include <typeinfo>
#include <vector>

#undef new

#ifdef NDEBUG
#define PushAllocInfo(...) 
#define PopAllocInfo() 
#else
#define PushAllocInfo(msg) g_debug_alloc->pushAllocInfo(msg)
#define PopAllocInfo() g_debug_alloc->popAllocInfo()
#endif

struct IAllocator {
    virtual ~IAllocator() {}
    virtual void init() {}
    virtual void cleanup() {}

    virtual void *alloc(size_t n) = 0;
    virtual void *allocDebug(size_t n, const char *type_name);
    virtual void dealloc(void *ptr) = 0;

    template<typename T>
    T *alloc(size_t count = 1) {
        return (T *)alloc(sizeof(T) * count);
    }

    template<typename T>
    T *allocDebug(size_t count = 1, const char *type_name = typeid(T).name()) {
        return (T *)allocDebug(sizeof(T) * count, type_name);
    }

    template<typename T, typename ...TArgs>
    T *make(TArgs &&...args) {
        T *ptr = (T *)allocDebug(sizeof(T), typeid(T).name());
        if (!ptr) return nullptr;
        return new (ptr) T(args...);
    }

    template<typename T, typename ...TArgs>
    T *makeArr(size_t n, TArgs &&...args) {
        uint8_t *ptr = (uint8_t *)allocDebug(sizeof(T) * n + sizeof(ArrayHead), typeid(T).name());
        if (!ptr) return nullptr;
        ArrayHead *head = (ArrayHead *)ptr;
        head->len = n;
        T *start = (T *)(ptr + sizeof(ArrayHead));
        for (size_t i = 0; i < n; ++i) {
            new (start + i) T(args...);
        }
        return start;
    }

    template<typename T>
    void destroy(T *&ptr) {
        if (!ptr) return;
        ptr->~T();
        dealloc((void *)ptr);
        ptr = nullptr;
    }

    template<typename T>
    void destroyArr(T *&ptr) {
        if (!ptr) return;
        uint8_t *buf = (uint8_t *)ptr;
        ArrayHead *head = (ArrayHead *)(buf - sizeof(ArrayHead));
        size_t n = head->len;
        for (size_t i = 0; i < n; ++i) {
            (ptr + i)->~T();
        }
        dealloc((void *)head);
        ptr = nullptr;
    }

protected:
    struct ArrayHead {
        size_t len;
    };
};

struct IDebugAllocator {
    struct AllocInfo {
        char name[64];
        char extra[64];
        size_t size;
        void *ptr;
    };
    std::vector<AllocInfo> alloc_info;

    virtual ~IDebugAllocator() {}
    virtual void pushAllocInfo(const char *msg) = 0;
    virtual void popAllocInfo() = 0;
};

struct SimpleAlloc : public IAllocator {
    virtual void *alloc(size_t n) override;
    virtual void dealloc(void *ptr) override;
};

struct DefaultAlloc;

extern IAllocator *g_alloc;
extern IDebugAllocator *g_debug_alloc;
