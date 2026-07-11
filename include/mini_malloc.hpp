#pragma once

#include <cstddef>

//这些函数的核心是维护空闲链表
//个人malloc函数
void* my_malloc(std::size_t size);

void my_free(void* ptr);

bool validate_heap();
void dump_heap();
