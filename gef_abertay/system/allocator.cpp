#include "allocator.h"

#include <unordered_map>
#include <string>
#include <assert.h>

#include <gef.h>
#include <string.h>

#include <system/debug_log.h>
#include <maths/math_utils.h>

#undef IS_DEBUG
#undef IS_RELEASE

#ifdef NDEBUG
#define IS_DEBUG 0
#define IS_RELEASE 1
#else
#define IS_DEBUG 1
#define IS_RELEASE 0
#endif

static_assert(IS_DEBUG != IS_RELEASE);

struct DebugInfo {
    std::string name;
    size_t size;
};

struct DebugAlloc : public IAllocator, public IDebugAllocator {
    virtual void init() override;
    virtual void cleanup() override;

    virtual void *alloc(size_t n) override;
    virtual void *allocDebug(size_t n, const char *type_name) override;
    virtual void dealloc(void *ptr) override;

    virtual void pushAllocInfo(const char *msg) override;
    virtual void popAllocInfo() override;

    void addAllocation(void *ptr, const DebugInfo &info);
    void removeAllocation(void *ptr);

    std::unordered_map<void *, DebugInfo> allocations;
    std::unordered_map<void *, DebugInfo> freed_allocs;
    std::vector<std::string> extra_info;
};

struct NullAlloc : public IAllocator, public IDebugAllocator {
    virtual void *alloc(size_t n) override { (void)n; return nullptr; }
    virtual void dealloc(void *ptr) override { (void)ptr; }
    virtual void pushAllocInfo(const char *msg) override { (void)msg; }
    virtual void popAllocInfo() override {}
};

static SimpleAlloc simple_alloc_inst;
#if IS_DEBUG
static DebugAlloc default_alloc_inst;
IAllocator *g_alloc = &default_alloc_inst;
IDebugAllocator *g_debug_alloc = &default_alloc_inst;
#else
static NullAlloc null_alloc_inst;
IAllocator *g_alloc = &simple_alloc_inst;
IDebugAllocator *g_debug_alloc = &null_alloc_inst;
#endif

void *IAllocator::allocDebug(size_t n, const char *type_name) {
    (void)type_name;
    return alloc(n);
}

void *SimpleAlloc::alloc(size_t n) {
    return calloc(1, n);
}

void SimpleAlloc::dealloc(void *ptr) {
    free(ptr);
}

void DebugAlloc::init() {
}

void DebugAlloc::cleanup() {
    assert(alloc_info.size() == allocations.size());
    if (alloc_info.empty()) {
        gef::DebugOut(">>>>>>>>>> ALL FREED! NO LEAK DETECTED! <<<<<<<<<<\n");
    }
    else {
        for (auto &info : alloc_info) {
            gef::DebugOut("[WARN] forgot to free %p (%s -> %s)\n", info.ptr, info.name, info.extra);
        }
    }
}

void *DebugAlloc::alloc(size_t n) {
    void *ptr = calloc(1, n);
    DebugInfo info = { "n/a", n };
    allocations[ptr] = info;
    addAllocation(ptr, info);
    return ptr;
}

void *DebugAlloc::allocDebug(size_t n, const char *type_name) {
    void *ptr = calloc(1, n);
    DebugInfo info = { type_name, n };
    allocations[ptr] = info;
    addAllocation(ptr, info);
    return ptr;
}

void DebugAlloc::dealloc(void *ptr) {
#if IS_RELEASE
    free(ptr);
#else
    if (!ptr) return;

    auto iter = allocations.find(ptr);
    if (iter != allocations.end()) {
        freed_allocs[ptr] = std::move(iter->second);
        allocations.erase(iter);
        free(ptr);
        removeAllocation(ptr);
        return;
    }
    iter = freed_allocs.find(ptr);
    // pointer has already been freed, double free error!
    if (iter != freed_allocs.end()) {
        gef::DebugOut("[FATAL] double free ptr: %p (%s)\n", iter->first, iter->second);
        abort();
    }
    // pointer not found, it wasn't allocated with this allocator, 
    // meaning that its probably an error
    gef::DebugOut("[FATAL] ptr not allocated with this allocator: %p\n", ptr);
    abort();
#endif
}

void DebugAlloc::pushAllocInfo(const char *msg) {
    extra_info.emplace_back(msg);
}

void DebugAlloc::popAllocInfo() {
    if (!extra_info.empty()) {
        extra_info.pop_back();
    }
}

void DebugAlloc::addAllocation(void *ptr, const DebugInfo &deb_info) {
    std::string extra;
    for (size_t i = 0; i < extra_info.size(); ++i) {
        extra += extra_info[i];
        if (i < (extra_info.size() - 1)) {
            extra += '|';
        }
    }

    if (extra.empty()) {
        extra = "(none)";
    }

    if (extra == "App") {
        int a = 2;
    }

    AllocInfo info;
    memset(&info, 0, sizeof(info));
    info.ptr = ptr;
    info.size = deb_info.size;
    strncpy_s(info.name, deb_info.name.c_str(), gef::min(sizeof(info.name) - 1, deb_info.name.size()));
    strncpy_s(info.extra, extra.c_str(), gef::min(sizeof(info.extra) - 1, extra.size()));
    
    // add allocation in sorted vector
    bool found = false;
    for (size_t i = 0; i < alloc_info.size(); ++i) {
        if (alloc_info[i].size < info.size) {
            found = true;
            alloc_info.emplace(alloc_info.begin() + i, info);
            break;
        }
    }

    if (!found) {
        alloc_info.emplace_back(info);
    }
}

void DebugAlloc::removeAllocation(void *ptr) {
    for (size_t i = 0; i < alloc_info.size(); ++i) {
        if (alloc_info[i].ptr == ptr) {
            alloc_info.erase(alloc_info.begin() + i);
            return;
        }
    }

    gef::DebugOut("[WARN] couldn't find allocation %p in out allocations\n", ptr);
}
