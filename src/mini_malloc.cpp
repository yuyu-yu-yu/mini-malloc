#include "mini_malloc.hpp"
#include "process_memory.hpp"

#include <cstdint>
#include <iostream>

namespace {

constexpr std::size_t kHeapSize = 1024 * 1024;  // 定义一个代表1MB大小的常量kHeapSize
alignas(16) unsigned char heap[kHeapSize];      // 申请一块 1MB 的原始内存，我们模拟管理的就是这块内存
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
    void *old_p = heap + old_size;
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
}

//my_malloc()和my_free()的核心都是维护空闲链表这个数据结构

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
    std::cout << "free list dump\n";

    if (freep == nullptr) {
        std::cout << "  <not initialized>\n";
        return;
    }

    const std::uintptr_t heap_begin = reinterpret_cast<std::uintptr_t>(heap);
    const std::size_t max_nodes = kHeapSize / sizeof(BlockHeader) + 1;

    BlockHeader* current = freep;
    std::size_t index = 0;

    do {
        if (current == nullptr) {
            std::cout << "  [" << index << "] nullptr node\n";
            return;
        }

        if (current == &base) {
            std::cout << "  [" << index << "] base sentinel"
                      << " next=" << current->ptr << "\n";
        } else {
            const std::uintptr_t addr =
                reinterpret_cast<std::uintptr_t>(current);
            const std::size_t offset = addr - heap_begin;
            const std::size_t bytes = current->size * sizeof(BlockHeader);

            std::cout << "  [" << index << "] offset=" << offset
                      << " size_units=" << current->size
                      << " size_bytes=" << bytes
                      << " next=" << current->ptr << "\n";
        }

        current = current->ptr;
        index++;
        if (index > max_nodes) {
            std::cout << "  <stop: possible cycle or corrupted free list>\n";
            return;
        }
    } while (current != freep);
}
