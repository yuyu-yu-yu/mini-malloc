#include "mini_malloc.hpp"
#include "process_memory.hpp"

#include <cstdint>
#include <iomanip>
#include <iostream>

namespace {

constexpr std::size_t kHeapSize = 1024 * 1024;  // 定义一个代表1MB大小的常量kHeapSize
alignas(16) unsigned char heap[kHeapSize];      // C++ 层面整块空间从一开始就已存在；本项目通过 current_process.size
                                                // 逻辑上规定目前只有前多少字节可以由分配器使用。
//std::size_t heap_used = 0;                      //表示目前已经被malloc管理的内存大小,这里是以字节为单位的,
//用current_process.size代替了  -v-

struct alignas(16) BlockHeader{
    std::size_t size;   // 表示该块空闲大小
    BlockHeader *ptr;   // 指向下一处空闲块
};

BlockHeader base;       //定义一个哨兵节点
BlockHeader *freep = nullptr;     //每次查找的起点

ProcessMemory current_process;  //当前模拟进程，维护size和page_table
bool memory_system_initialized = false; //物理页分配器和进程内存是否已经初始化

void* sim_sbrk(std::size_t bytes) {
    if(memory_system_initialized == false){
        kinit();
        process_memory_init(current_process);
        memory_system_initialized = true;
    }
    std::size_t old_size = current_process.size;    //记录旧size
    if(growproc(current_process, bytes) == false)
        return nullptr;
    void *old_p = heap + old_size;  //这里实际上放开了heap这个空间的使用权限，我们并没有真正的去申请内存，而是通过growproc()来维护当前进程的虚拟地址空间大小，模拟了sbrk()的功能
    return old_p;
}

BlockHeader* morecore(std::size_t nunits) {
    constexpr std::size_t kPageSize = 4096;
    std::size_t min_units = kPageSize / sizeof(BlockHeader);

    //最少申请一页
    if(nunits < min_units)
        nunits = min_units;
    std::size_t byte = nunits*sizeof(BlockHeader);
    
    BlockHeader *p = (BlockHeader*)sim_sbrk(byte);
    if(p == nullptr)
        return nullptr;
    p->size = nunits;
    my_free(p + 1); //因为my_free会通过传入的地址找回头部
    return freep;   //因为p可能被合并了
}
}//匿名空间结束

//my_malloc()和my_free()的核心都是维护空闲链表这个数据结构，这里的空闲链表指的是每一块的BlockHeader

void* my_malloc(std::size_t size) {
    if(size == 0 || size > kHeapSize){
        return nullptr;
    }
    BlockHeader *p, *prep;
    prep = freep;
    std::size_t nunits = (size + sizeof(BlockHeader) - 1) / sizeof(BlockHeader) + 1; //记录应当申请的大小（BlockHeader倍数）

    //初始化
    if(freep == nullptr){
        //BlockHeader* up = (BlockHeader*)heap; 
        base.ptr = &base;
        base.size = 0;
    
        //up->size = kHeapSize / sizeof(BlockHeader);
        //up->ptr = &base;
    
        freep = prep = &base; 
    }
    for(p = prep->ptr;;prep = p, p = p->ptr){
        if(p->size >= nunits){
            if(p->size == nunits){
                prep->ptr = p->ptr;
            }
            else{
                p->size -= nunits;
                //后面要给分出去的这一块记录大小信息
                prep = p;
                p += p->size;
                p->size = nunits;
            }
            freep = prep;
            return (void*)(p + 1);//返回时跳过头部
        }
        if(p == freep)
            //这里后续补充内存扩充函数
            //已补充
            if((p = morecore(nunits)) == nullptr)
                return nullptr;
    }

}

void my_free(void* ptr) {
    if(ptr == nullptr)
        return;
    BlockHeader *bp, *p;
    bp = (BlockHeader*)ptr - 1;

    //定位，找到bp的前置节点
    for(p = freep;!(p < bp && bp < p->ptr);p = p->ptr){
        if(p >= p->ptr && (bp > p || bp < p->ptr))
            break;
    }

    bp->ptr = p->ptr;
    p->ptr = bp;

    if(bp + bp->size == bp->ptr && bp->ptr != &base){
        bp->size += bp->ptr->size;
        bp->ptr = bp->ptr->ptr;
    }
    if(p + p->size == bp){
        p->size += bp->size;
        p->ptr = bp->ptr;
    }

    freep = p;
}

// 后面的两个功能调试函数由AI给出
bool validate_heap() {
    if (freep == nullptr) {
        return true;
    }

    const std::uintptr_t heap_begin = reinterpret_cast<std::uintptr_t>(heap);
    const std::uintptr_t heap_end = heap_begin + kHeapSize;
    const std::size_t max_nodes = kHeapSize / sizeof(BlockHeader) + 1;

    if (base.size != 0) {
        return false;
    }

    BlockHeader* current = freep;
    std::size_t seen = 0;

    do {
        if (current == nullptr || current->ptr == nullptr) {
            return false;
        }

        if (current != &base) {
            const std::uintptr_t addr =
                reinterpret_cast<std::uintptr_t>(current);

            if (addr < heap_begin || addr + sizeof(BlockHeader) > heap_end) {
                return false;
            }
            if ((addr - heap_begin) % sizeof(BlockHeader) != 0) {
                return false;
            }
            if (current->size == 0) {
                return false;
            }
            if (current->size > kHeapSize / sizeof(BlockHeader)) {
                return false;
            }
            if (addr + current->size * sizeof(BlockHeader) > heap_end) {
                return false;
            }
        }

        current = current->ptr;
        seen++;
        if (seen > max_nodes) {
            return false;
        }
    } while (current != freep);

    return true;
}

void dump_heap() {
    std::cout << "用户态空闲链表：\n";

    if (freep == nullptr) {
        std::cout << "  <尚未初始化>\n";
        return;
    }

    const std::uintptr_t heap_begin = reinterpret_cast<std::uintptr_t>(heap);
    const std::size_t max_nodes = kHeapSize / sizeof(BlockHeader) + 1;

    BlockHeader* current = freep;
    std::size_t index = 0;

    do {
        if (current == nullptr) {
            std::cout << "  [" << index << "] 空指针节点\n";
            return;
        }

        if (current == &base) {
            std::cout << "  [" << index << "] 哨兵 base"
                      << "，下一节点=" << current->ptr << "\n";
        } else {
            const std::uintptr_t addr =
                reinterpret_cast<std::uintptr_t>(current);
            const std::size_t offset = addr - heap_begin;
            const std::size_t bytes = current->size * sizeof(BlockHeader);

            std::cout << "  [" << index << "] heap偏移=" << offset
                      << "B，大小=" << current->size
                      << "个BlockHeader=" << bytes
                      << "B，下一节点=" << current->ptr << "\n";
        }

        current = current->ptr;
        index++;
        if (index > max_nodes) {
            std::cout << "  <停止：链表可能存在异常循环或结构损坏>\n";
            return;
        }
    } while (current != freep);
}

bool validate_memory_system() {
    if (!validate_heap()) {
        return false;
    }

    if (!memory_system_initialized) {
        return true;
    }

    return validate_page_allocator()
        && validate_page_table(current_process.page_table)
        && current_process.size <= kHeapSize;
}

void dump_pointer_mapping(const void* ptr) {
    std::cout << "返回指针的地址关系：\n";

    if (ptr == nullptr) {
        std::cout << "  返回指针：nullptr\n";
        return;
    }

    const std::uintptr_t heap_begin =
        reinterpret_cast<std::uintptr_t>(heap);
    const std::uintptr_t heap_end = heap_begin + kHeapSize;
    const std::uintptr_t address =
        reinterpret_cast<std::uintptr_t>(ptr);

    std::cout << "  malloc实际返回的heap地址：" << ptr << "\n";

    if (address < heap_begin || address >= heap_end) {
        std::cout << "  <该指针不在heap数组中>\n";
        return;
    }

    const std::size_t virtual_address = address - heap_begin;
    std::cout << "  heap起始地址：" << static_cast<void*>(heap) << "\n"
              << "  模拟用户虚拟地址（heap偏移）：0x"
              << std::hex << virtual_address << std::dec
              << "，即 " << virtual_address << "B\n";

    if (!memory_system_initialized
        || virtual_address >= current_process.size) {
        std::cout << "  <该位置尚未包含在进程的逻辑可用空间中>\n";
        return;
    }

    const std::size_t page_index = virtual_address / kPageSize;
    const std::size_t page_offset = virtual_address % kPageSize;
    const PageTableEntry& entry =
        current_process.page_table.entries[page_index];

    std::cout << "  虚拟页号：" << page_index << "\n"
              << "  页内偏移：0x" << std::hex << page_offset
              << std::dec << "，即 " << page_offset << "B\n";

    if (!entry.valid || entry.physical_page == nullptr) {
        std::cout << "  模拟页表项：未映射\n";
        return;
    }

    const std::uintptr_t physical_page =
        reinterpret_cast<std::uintptr_t>(entry.physical_page);
    const std::uintptr_t simulated_physical_address =
        physical_page + page_offset;

    std::cout << "  页表映射：虚拟页" << page_index
              << " -> 模拟物理页 " << entry.physical_page << "\n"
              << "  模拟物理地址：0x" << std::hex
              << simulated_physical_address << std::dec << "\n"
              << "  说明：C++实际读写heap地址；模拟物理地址只记录映射关系。\n";
}

void dump_memory_system() {
    std::cout << "\n========== 模拟内存系统快照 ==========\n";
    std::cout << "初始化状态："
              << (memory_system_initialized ? "已初始化" : "尚未初始化")
              << "\n";
    std::cout << "heap容量：" << kHeapSize << "B\n";

    if (memory_system_initialized) {
        std::cout << "当前进程逻辑大小：" << current_process.size << "B\n";
        std::cout << "当前映射页数："
                  << (current_process.size + kPageSize - 1) / kPageSize
                  << "\n";
    }

    std::cout << "\n[1] 用户态小块分配器\n";
    dump_heap();

    std::cout << "\n[2] 模拟物理页分配器\n";
    if (memory_system_initialized) {
        dump_page_allocator();
    } else {
        std::cout << "物理页分配器尚未初始化。\n";
    }

    std::cout << "\n[3] 模拟进程页表\n";
    if (memory_system_initialized) {
        dump_page_table(current_process.page_table);
    } else {
        std::cout << "进程页表尚未初始化。\n";
    }

    std::cout << "\n[4] 结构校验\n";
    std::cout << "  用户态空闲链表："
              << (validate_heap() ? "通过" : "失败") << "\n";
    if (memory_system_initialized) {
        std::cout << "  模拟物理页分配器："
                  << (validate_page_allocator() ? "通过" : "失败")
                  << "\n";
        std::cout << "  模拟进程页表："
                  << (validate_page_table(current_process.page_table)
                      ? "通过" : "失败")
                  << "\n";
    }
    std::cout << "  内存系统整体："
              << (validate_memory_system() ? "通过" : "失败") << "\n";
    std::cout << "======================================\n";
}
