#include "page_allocator.hpp"

#include <cstdint>
#include <iostream>

namespace {

constexpr std::size_t kPhysicalMemorySize = 1024 * 1024;

//用这个空间来模拟真实的物理内存
alignas(kPageSize) unsigned char physical_memory[kPhysicalMemorySize];

struct FreePage {
    FreePage* next;
};

FreePage* free_pages = nullptr;

std::size_t free_pages_count = 0;

}

void kinit(){
    free_pages = nullptr;
    free_pages_count = 0;
    unsigned char *p = physical_memory;
    for(; p < physical_memory + kPhysicalMemorySize; p += kPageSize){
        kfree(p);
    }
}

void* kalloc(){
    FreePage* p;
    p = free_pages;
    if(p != nullptr){
        free_pages = free_pages->next;
        free_pages_count--;
    }
    return (void*)p;
}

void kfree(void* page){
    if(page == nullptr)
        return;

    std::uintptr_t address =
    reinterpret_cast<std::uintptr_t>(page); //std::uintptr_t：为指针定制的整数类型，使之可以进行数学运算

    //物理地址的起始
    std::uintptr_t begin =
    reinterpret_cast<std::uintptr_t>(physical_memory);

    //物理地址的结束
    std::uintptr_t last =
    begin + kPhysicalMemorySize - kPageSize;

    if (address < begin ||
        address > last ||(address - begin) % kPageSize != 0) {
        return;
    }
    /*
    if((std::uintptr_t)page % kPageSize != 0|| page < physical_memory 
    || page > physical_memory + kPhysicalMemorySize - kPageSize){
        return;
    }
    */

    FreePage* p;
    //这里后续可以补充一个memset函数，初始化值
    p = (FreePage*) page;
    p->next = free_pages;
    free_pages = p;

    free_pages_count++;
}

//后面的调试函数由AI给出

std::size_t free_page_count(){
    return free_pages_count;
}

bool validate_page_allocator(){
    constexpr std::size_t kTotalPages =
        kPhysicalMemorySize / kPageSize;

    if (free_pages_count > kTotalPages) {
        return false;
    }

    const std::uintptr_t begin =
        reinterpret_cast<std::uintptr_t>(physical_memory);
    const std::uintptr_t end = begin + kPhysicalMemorySize;
    bool visited[kTotalPages] = {};

    std::size_t count = 0;
    for (FreePage* page = free_pages; page != nullptr; page = page->next) {
        const std::uintptr_t address =
            reinterpret_cast<std::uintptr_t>(page);

        if (address < begin || address >= end) {
            return false;
        }

        const std::uintptr_t offset = address - begin;
        if (offset % kPageSize != 0) {
            return false;
        }

        const std::size_t page_index = offset / kPageSize;
        if (visited[page_index]) {
            return false;
        }

        visited[page_index] = true;
        count++;
    }

    return count == free_pages_count;
}

void dump_page_allocator(){
    constexpr std::size_t kTotalPages =
        kPhysicalMemorySize / kPageSize;
    const std::uintptr_t begin =
        reinterpret_cast<std::uintptr_t>(physical_memory);
    const std::uintptr_t end = begin + kPhysicalMemorySize;
    bool visited[kTotalPages] = {};

    std::cout << "page allocator: free=" << free_pages_count
              << "/" << kTotalPages << "\n";

    if (free_pages == nullptr) {
        std::cout << "  <empty>\n";
        return;
    }

    FreePage* page = free_pages;
    std::size_t list_index = 0;

    while (page != nullptr) {
        const std::uintptr_t address =
            reinterpret_cast<std::uintptr_t>(page);

        if (address < begin || address >= end) {
            std::cout << "  [" << list_index
                      << "] invalid address=" << page << "\n";
            return;
        }

        const std::uintptr_t offset = address - begin;
        if (offset % kPageSize != 0) {
            std::cout << "  [" << list_index
                      << "] unaligned address=" << page << "\n";
            return;
        }

        const std::size_t page_index = offset / kPageSize;
        if (visited[page_index]) {
            std::cout << "  [" << list_index << "] page=" << page_index
                      << " <duplicate or cycle>\n";
            return;
        }

        visited[page_index] = true;
        std::cout << "  [" << list_index << "] page=" << page_index
                  << " address=" << page
                  << " next=" << page->next << "\n";

        page = page->next;
        list_index++;
    }
}
