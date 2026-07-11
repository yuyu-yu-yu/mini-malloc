#include "process_memory.hpp"

#include <cstdint>
#include <iostream>

//进程大小为0
//256个虚拟页全部未映射
void process_memory_init(ProcessMemory& process){
    process.size = 0;
    page_table_init(process.page_table);
}

//把进程虚拟地址空间扩大 bytes 字节。
bool growproc(
    ProcessMemory& process,
    std::size_t bytes
){
    if (process.size > kVirtualMemorySize || bytes > kVirtualMemorySize - process.size) 
        return false;

    if(
    uvmalloc(process.page_table, process.size, process.size + bytes) == false
    )
        return false;
    process.size += bytes;
    return true;
}
