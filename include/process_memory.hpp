#pragma once

#include "virtual_memory.hpp"

#include <cstddef>

struct ProcessMemory {
    PageTable page_table;   //该进程的页表
    std::size_t size;       //该进程的虚拟地址空间大小，单位是字节
};

//初始化，进程大小为0,256个虚拟页全部未映射
void process_memory_init(ProcessMemory& process);

//把进程虚拟地址空间扩大 bytes 字节。
bool growproc(
    ProcessMemory& process,
    std::size_t bytes
);