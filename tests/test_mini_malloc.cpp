#include "mini_malloc.hpp"

#include <cstdint>
#include <iostream>

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            std::cerr << "失败: " << #condition << "，位置 " << __FILE__ << ":" \
                      << __LINE__ << "\n";                                    \
            dump_heap();                                                       \
            return false;                                                      \
        }                                                                      \
    } while (0)

static bool is_aligned_16(void* ptr) {
    return reinterpret_cast<std::uintptr_t>(ptr) % 16 == 0;
}

static bool test_malloc_zero_returns_nullptr() {
    CHECK(my_malloc(0) == nullptr);
    CHECK(validate_heap());
    return true;
}

static bool test_first_allocation_returns_pointer() {
    void* p = my_malloc(100);
    CHECK(p != nullptr);
    CHECK(is_aligned_16(p));
    CHECK(validate_heap());
    return true;
}

int main() {
    int passed = 0;
    int total = 0;

    total++;
    if (test_malloc_zero_returns_nullptr()) {
        passed++;
        std::cout << "通过: malloc(0) 返回 nullptr\n";
    }

    total++;
    if (test_first_allocation_returns_pointer()) {
        passed++;
        std::cout << "通过: 第一次分配返回 16 字节对齐的非空指针\n";
    }

    std::cout << "测试结果: " << passed << "/" << total << " 通过\n";
    return passed == total ? 0 : 1;
}
