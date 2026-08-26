/**
 * @file main.cpp
 * @brief GoogleTest 运行入口与全局堆内存分配劫持监视器
 */
#include <gtest/gtest.h>
#include <atomic>
#include <cstdlib>
#include <new>
#include <cstddef>

namespace alib6::test {
    inline std::atomic<std::size_t> g_global_heap_allocations{0};
    inline std::atomic<std::size_t> g_global_heap_allocated_bytes{0};
    inline std::atomic<bool> g_track_heap_allocations{false};

    void start_heap_allocation_tracking() {
        g_track_heap_allocations.store(true, std::memory_order_seq_cst);
    }

    void stop_heap_allocation_tracking() {
        g_track_heap_allocations.store(false, std::memory_order_seq_cst);
    }

    std::size_t get_global_heap_allocation_count() {
        return g_global_heap_allocations.load(std::memory_order_relaxed);
    }

    std::size_t get_global_heap_allocated_bytes() {
        return g_global_heap_allocated_bytes.load(std::memory_order_relaxed);
    }

    void reset_global_heap_allocation_count() {
        g_global_heap_allocations.store(0, std::memory_order_relaxed);
        g_global_heap_allocated_bytes.store(0, std::memory_order_relaxed);
    }
}

void* operator new(std::size_t size) {
    if (alib6::test::g_track_heap_allocations.load(std::memory_order_relaxed)) {
        alib6::test::g_global_heap_allocations.fetch_add(1, std::memory_order_relaxed);
        alib6::test::g_global_heap_allocated_bytes.fetch_add(size, std::memory_order_relaxed);
    }
    void* ptr = std::malloc(size);
    if (!ptr) throw std::bad_alloc();
    return ptr;
}

void* operator new[](std::size_t size) {
    if (alib6::test::g_track_heap_allocations.load(std::memory_order_relaxed)) {
        alib6::test::g_global_heap_allocations.fetch_add(1, std::memory_order_relaxed);
        alib6::test::g_global_heap_allocated_bytes.fetch_add(size, std::memory_order_relaxed);
    }
    void* ptr = std::malloc(size);
    if (!ptr) throw std::bad_alloc();
    return ptr;
}

void operator delete(void* ptr) noexcept {
    std::free(ptr);
}

void operator delete[](void* ptr) noexcept {
    std::free(ptr);
}

void operator delete(void* ptr, std::size_t) noexcept {
    std::free(ptr);
}

void operator delete[](void* ptr, std::size_t) noexcept {
    std::free(ptr);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
