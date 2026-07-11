#include "virtual_memory.hpp"

#include <cstdint>
#include <iostream>

//把页表项全部设置为未映射
void page_table_init(PageTable& page_table){
    for(std::size_t i = 0; i < kVirtualPageCount; i++){
        page_table.entries[i].valid = false;
        page_table.entries[i].physical_page = nullptr;
    }
}

//把一个页表项从未映射改成已映射
bool mappages(
    PageTable& page_table,
    std::size_t virtual_address,
    void* physical_page
){
    std::size_t v = virtual_address / kPageSize;    //虚拟页号
    if(v >= kVirtualPageCount || virtual_address % kPageSize != 0 || physical_page == nullptr
           ||reinterpret_cast<std::uintptr_t>(physical_page) % kPageSize != 0)
        return false;
    if(page_table.entries[v].valid == true)
        return false;

    page_table.entries[v].valid = true;
    page_table.entries[v].physical_page = physical_page;
    return true;
}

// 扩大或缩小用户虚拟地址空间，内部使用 kalloc/kfree 和 mappages。以字节为单位
bool uvmalloc(
    PageTable& page_table, 
    std::size_t old_size, 
    std::size_t new_size){
        if(old_size > new_size || new_size > kVirtualMemorySize)
            return false;
        std::size_t old_page = (old_size + kPageSize - 1) / kPageSize;
        std::size_t new_page = (new_size + kPageSize - 1) / kPageSize;
        if(new_page > kVirtualPageCount)
            return false;

        std::size_t i = new_page - old_page;
        std::size_t mapped_end = old_page * kPageSize; //用来记录以及成功分配的页面，方便后面回滚逻辑

        for(std::size_t j = 0; j < i; j++){
            void* phy = kalloc();

            if(phy == nullptr){
                uvmdealloc(page_table, mapped_end, old_size);
                return false;
            }

            if(!mappages(page_table, (old_page + j) * kPageSize, phy)){
                kfree(phy);
                uvmdealloc(page_table, mapped_end, old_size);
                return false;
            }
            mapped_end += kPageSize;
        }

        return true;
}

void uvmdealloc(
    PageTable& page_table,
    std::size_t old_size,
    std::size_t new_size
){
        if(old_size < new_size || old_size > kVirtualMemorySize || new_size > kVirtualMemorySize)
            return ;
        std::size_t old_page = (old_size + kPageSize - 1) / kPageSize;
        std::size_t new_page = (new_size + kPageSize - 1) / kPageSize;

        std::size_t i = old_page - new_page;
        for(std::size_t j = 0; j < i; j++){
            std::size_t page_index = new_page + j;

            kfree(page_table.entries[page_index].physical_page);
            page_table.entries[page_index].valid = false;
            page_table.entries[page_index].physical_page = nullptr;
        }
}

bool validate_page_table(const PageTable& page_table){
    for(std::size_t i = 0; i < kVirtualPageCount; i++){
        const PageTableEntry& entry = page_table.entries[i];

        if(!entry.valid){
            if(entry.physical_page != nullptr)
                return false;
            continue;
        }

        if(entry.physical_page == nullptr)
            return false;

        const std::uintptr_t physical_address =
            reinterpret_cast<std::uintptr_t>(entry.physical_page);
        if(physical_address % kPageSize != 0)
            return false;

        for(std::size_t j = 0; j < i; j++){
            const PageTableEntry& previous = page_table.entries[j];
            if(previous.valid &&
               previous.physical_page == entry.physical_page)
                return false;
        }
    }

    return true;
}

void dump_page_table(const PageTable& page_table){
    std::size_t mapped_pages = 0;
    std::size_t invalid_entries = 0;

    std::cout << "页表映射信息：\n";

    for(std::size_t i = 0; i < kVirtualPageCount; i++){
        const PageTableEntry& entry = page_table.entries[i];

        if(!entry.valid && entry.physical_page == nullptr)
            continue;

        const std::size_t virtual_begin = i * kPageSize;
        const std::size_t virtual_end = virtual_begin + kPageSize - 1;

        std::cout << "  虚拟页 " << i
                  << " [" << virtual_begin << ", " << virtual_end << "]"
                  << " -> " << entry.physical_page;

        bool entry_ok = entry.valid && entry.physical_page != nullptr;
        if(entry_ok){
            const std::uintptr_t physical_address =
                reinterpret_cast<std::uintptr_t>(entry.physical_page);
            entry_ok = physical_address % kPageSize == 0;
        }

        bool duplicated = false;
        if(entry.valid && entry.physical_page != nullptr){
            for(std::size_t j = 0; j < i; j++){
                const PageTableEntry& previous = page_table.entries[j];
                if(previous.valid &&
                   previous.physical_page == entry.physical_page){
                    duplicated = true;
                    break;
                }
            }
        }

        if(!entry_ok){
            std::cout << "  <页表项状态异常>";
            invalid_entries++;
        }else if(duplicated){
            std::cout << "  <物理页重复映射>";
            invalid_entries++;
        }else{
            mapped_pages++;
        }

        std::cout << "\n";
    }

    std::cout << "汇总：有效映射 " << mapped_pages
              << " 页，异常 " << invalid_entries << " 项\n";
}
