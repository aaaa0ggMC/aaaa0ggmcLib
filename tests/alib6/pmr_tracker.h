/**
 * @file pmr_tracker.h
 * @brief PMR 内存池追踪器，用于单测中检测内存分配路由与内存泄漏
 */
#pragma once
#include <gtest/gtest.h>
#include <memory_resource>
#include <cstddef>

namespace alib6::test {

    /**
     * @brief 统计型 PMR 内存资源，用于精确验证内存是否走入指定内存池及析构时是否彻底归还
     */
    class CountingMemoryResource : public std::pmr::memory_resource {
    private:
        std::pmr::memory_resource* upstream_{std::pmr::get_default_resource()};
        std::size_t allocated_bytes_{0};
        std::size_t deallocated_bytes_{0};
        std::size_t allocation_count_{0};
        std::size_t deallocation_count_{0};

    public:
        explicit CountingMemoryResource(std::pmr::memory_resource* upstream = std::pmr::get_default_resource())
            : upstream_(upstream) {}

        [[nodiscard]] std::size_t allocated_bytes() const noexcept { return allocated_bytes_; }
        [[nodiscard]] std::size_t deallocated_bytes() const noexcept { return deallocated_bytes_; }
        [[nodiscard]] std::size_t allocation_count() const noexcept { return allocation_count_; }
        [[nodiscard]] std::size_t deallocation_count() const noexcept { return deallocation_count_; }
        [[nodiscard]] std::size_t live_bytes() const noexcept { return allocated_bytes_ - deallocated_bytes_; }
        [[nodiscard]] bool has_leak() const noexcept { return allocated_bytes_ != deallocated_bytes_; }

    protected:
        void* do_allocate(std::size_t bytes, std::size_t alignment) override {
            allocated_bytes_ += bytes;
            ++allocation_count_;
            return upstream_->allocate(bytes, alignment);
        }

        void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) override {
            deallocated_bytes_ += bytes;
            ++deallocation_count_;
            upstream_->deallocate(p, bytes, alignment);
        }

        bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
            return this == &other;
        }
    };

    void start_heap_allocation_tracking();
    void stop_heap_allocation_tracking();
    std::size_t get_global_heap_allocation_count();
    std::size_t get_global_heap_allocated_bytes();
    void reset_global_heap_allocation_count();

    /**
     * @brief 全局堆内存分配监视守卫
     * 处于该 Guard 作用域内的代码，如果触发了任何非 PMR 的全局堆分配（通过 operator new），
     * 均会被计数并可通过 new_allocations() 校验。
     */
    struct HeapAllocationGuard {
        std::size_t initial_count{0};
        explicit HeapAllocationGuard() {
            start_heap_allocation_tracking();
            initial_count = get_global_heap_allocation_count();
        }

        [[nodiscard]] std::size_t new_allocations() const noexcept {
            return get_global_heap_allocation_count() - initial_count;
        }

        ~HeapAllocationGuard() {
            stop_heap_allocation_tracking();
        }
    };

} // namespace alib6::test
