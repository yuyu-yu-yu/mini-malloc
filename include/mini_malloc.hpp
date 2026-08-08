#pragma once

#include <cstddef>

//这些函数的核心是维护空闲链表
//个人malloc函数
void* my_malloc(std::size_t size);

void my_free(void* ptr);

bool validate_heap();
void dump_heap();

// 调试辅助函数：显示整个模拟内存系统，以及一个返回指针的地址映射关系。
bool validate_memory_system();
void dump_memory_system();
void dump_pointer_mapping(const void* ptr);
