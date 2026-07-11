#pragma once

#include "page_allocator.hpp"

#include <cstddef>

//页表项
struct PageTableEntry {
    void* physical_page;
    bool valid;
    //虚拟页号直接由数组下标代替
};

//页表项的数目
constexpr std::size_t kVirtualPageCount =
    1024 * 1024 / kPageSize;

constexpr std::size_t  kVirtualMemorySize = 
    1024 * 1024;
//页表
struct PageTable {
    PageTableEntry entries[kVirtualPageCount];
};


// 初始化页表，清空页表中的全部映射。
void page_table_init(PageTable& page_table);

// 将一个页对齐的虚拟地址映射到一个物理页。
// 映射成功返回 true；参数非法或该虚拟页已映射时返回 false。
bool mappages(
    PageTable& page_table,
    std::size_t virtual_address,
    void* physical_page
);

// 扩大或缩小用户虚拟地址空间，内部使用 kalloc/kfree 和 mappages。
bool uvmalloc(
    PageTable& page_table,
    std::size_t old_size,
    std::size_t new_size
);
void uvmdealloc(
    PageTable& page_table,
    std::size_t old_size,
    std::size_t new_size
);

// 调试辅助函数。
bool validate_page_table(const PageTable& page_table);
void dump_page_table(const PageTable& page_table);
