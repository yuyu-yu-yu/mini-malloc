#pragma once

#include <cstddef>

constexpr std::size_t kPageSize = 4096;

//初始化物理页
void kinit();
//分配物理页
void* kalloc();
//释放物理页
void kfree(void* page);

std::size_t free_page_count();
bool validate_page_allocator();
void dump_page_allocator();