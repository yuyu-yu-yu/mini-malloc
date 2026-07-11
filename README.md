# mini-malloc：从 `malloc` 到物理页的内存分配链路模拟

本项目是一个学习型 C++ 内存管理项目，模拟xv-6的内存管理。部分调试函数由AI给出，但全部内存分配函数为个人所写。项目从用户态空闲链表开始，逐层实现进程地址空间扩展、虚拟页映射和物理页分配，最终串起以下调用链：

```text
my_malloc
→ morecore
→ sim_sbrk
→ growproc
→ uvmalloc
→ kalloc + mappages
```

项目使用普通 C++ 程序模拟这些组件，不会真正切换 CPU 权限级别，也不会修改硬件页表。

## 整体结构

```mermaid
flowchart TD
    A["my_malloc：分配小内存块"] --> B["morecore：用户堆空间不足"]
    B --> C["sim_sbrk：模拟 sbrk"]
    C --> D["growproc：扩大进程地址空间"]
    D --> E["uvmalloc：增加虚拟页"]
    E --> F["kalloc：取得物理页"]
    E --> G["mappages：建立页表映射"]
    F --> G
    G --> H["morecore 获得新的虚拟堆区间"]
    H --> I["my_free：将新区间加入空闲链表"]
    I --> A
```

项目分为四层：

```text
用户态小块分配：my_malloc / my_free / morecore
系统调用模拟：  sim_sbrk
进程虚拟内存：  growproc / uvmalloc / mappages
物理页分配：    kalloc / kfree
```

## 目录结构

```text
mini-malloc/
├─ include/
│  ├─ mini_malloc.hpp
│  ├─ page_allocator.hpp
│  ├─ virtual_memory.hpp
│  └─ process_memory.hpp
├─ src/
│  ├─ mini_malloc.cpp
│  ├─ page_allocator.cpp
│  ├─ virtual_memory.cpp
│  └─ process_memory.cpp
├─ tests/
│  └─ test_mini_malloc.cpp
└─ README.md
```

## 用户态分配器

文件：`src/mini_malloc.cpp`、`include/mini_malloc.hpp`

这一层管理大小不同的小内存块，负责查找、分割、释放和合并。它对应 xv6 的 `user/umalloc.c`。

### 数据结构

```cpp
alignas(16) unsigned char heap[1024 * 1024];

struct alignas(16) BlockHeader {
    std::size_t size;
    BlockHeader* ptr;
};

BlockHeader base;
BlockHeader* freep;
```

- `heap`：用户程序看到的 1MB 连续虚拟堆。
- `BlockHeader::size`：内存块大小，单位是 `BlockHeader`，包含头部本身。
- `BlockHeader::ptr`：指向下一个空闲块。
- `base`：循环空闲链表的哨兵，不属于真实堆内存。
- `freep`：下一次查找空闲块的起点。

该文件还保存：

```cpp
ProcessMemory current_process;
bool memory_system_initialized;
```

- `current_process`：当前模拟进程的大小和页表。
- `memory_system_initialized`：保证物理内存和进程只初始化一次。

### 函数

| 函数 | 作用 |
|---|---|
| `my_malloc(size)` | 在空闲链表中查找合适块，必要时分割，返回跳过头部的用户地址 |
| `my_free(ptr)` | 将内存块按地址插回空闲链表，并合并前后相邻块 |
| `morecore(nunits)` | 空闲链表不足时，至少申请一个 4096B 页面，并将新区间加入空闲链表 |
| `sim_sbrk(bytes)` | 保存旧堆顶，调用 `growproc`，成功后返回 `heap + old_size` |
| `validate_heap()` | 检查空闲链表指针、块范围、大小和循环结构 |
| `dump_heap()` | 打印空闲块偏移、大小和下一个节点 |

## 物理页分配器

文件：`src/page_allocator.cpp`、`include/page_allocator.hpp`

这一层将 1MB 模拟物理内存切成 256 个固定大小的页面，对应 xv6 的 `kernel/kalloc.c`。

### 数据结构

```cpp
alignas(4096) unsigned char physical_memory[1024 * 1024];

struct FreePage {
    FreePage* next;
};

FreePage* free_pages;
std::size_t free_pages_count;
```

- `physical_memory`：模拟的 1MB 物理内存。
- `FreePage`：空闲页面开头保存的侵入式链表节点。
- `free_pages`：空闲物理页链表头。
- `free_pages_count`：当前空闲页数量。

空闲页面使用自己的前几个字节保存 `next`，不额外申请链表节点。

### 函数

| 函数 | 作用 |
|---|---|
| `kinit()` | 将物理内存切成 256 个 4096B 页面并加入空闲链表 |
| `kalloc()` | 从链表头取出一个物理页；没有空闲页时返回 `nullptr` |
| `kfree(page)` | 检查范围和对齐后，将物理页放回链表头 |
| `free_page_count()` | 返回当前空闲页数量 |
| `validate_page_allocator()` | 检查页面范围、对齐、重复节点、环和计数 |
| `dump_page_allocator()` | 打印空闲物理页链表 |

## 虚拟内存和页表

文件：`src/virtual_memory.cpp`、`include/virtual_memory.hpp`

这一层维护虚拟页到物理页的映射，对应 xv6 的 `kernel/vm.c`。

### 数据结构

```cpp
struct PageTableEntry {
    void* physical_page;
    bool valid;
};

struct PageTable {
    PageTableEntry entries[256];
};
```

- `physical_page`：该虚拟页对应的物理页起始地址。
- `valid`：页表项是否有效，对应简化的 `PTE_V`。
- `entries` 的数组下标：虚拟页号。

例如：

```text
entries[0]：虚拟地址 0～4095
entries[1]：虚拟地址 4096～8191
entries[2]：虚拟地址 8192～12287
```

### 函数

| 函数 | 作用 |
|---|---|
| `page_table_init()` | 将全部页表项设为 `{nullptr, false}` |
| `mappages()` | 将一个页对齐的虚拟地址映射到一个物理页 |
| `uvmalloc()` | 扩大虚拟地址空间，逐页执行 `kalloc + mappages`，失败时回滚 |
| `uvmdealloc()` | 缩小虚拟地址空间，清除映射并通过 `kfree` 归还物理页 |
| `validate_page_table()` | 检查有效位、空指针、物理页对齐和重复映射 |
| `dump_page_table()` | 打印虚拟页号、虚拟地址范围和物理页地址 |

`mappages` 只登记一页映射，不负责申请物理页；`uvmalloc` 负责调用 `kalloc`。

## 进程内存

文件：`src/process_memory.cpp`、`include/process_memory.hpp`

这一层维护单个进程的页表和地址空间大小，对应 xv6 的 `growproc`。

### 数据结构

```cpp
struct ProcessMemory {
    PageTable page_table;
    std::size_t size;
};
```

- `page_table`：该进程的虚拟页映射。
- `size`：该进程当前拥有的准确字节数，不是向上取整后的页大小。

### 函数

| 函数 | 作用 |
|---|---|
| `process_memory_init()` | 将进程大小设为 0，并初始化页表 |
| `growproc(bytes)` | 检查空间上限，调用 `uvmalloc`，成功后更新进程大小 |

`growproc` 只在 `uvmalloc` 成功后更新 `size`，保证进程大小和页表状态一致。

## 一次分配如何完成

假设 `sizeof(BlockHeader) == 16`，程序第一次调用：

```cpp
void* ptr = my_malloc(100);
```

执行过程如下：

1. `my_malloc` 将 100B 向上取整，并加上一个块头，共需要 8 个 `BlockHeader` 单位，即 128B。
2. 空闲链表只有哨兵，`my_malloc` 调用 `morecore`。
3. `morecore` 决定至少扩展一页，即 4096B。
4. `sim_sbrk` 首次运行时调用 `kinit` 和 `process_memory_init`。
5. `sim_sbrk` 保存旧进程大小 0，并调用 `growproc(4096)`。
6. `growproc` 调用 `uvmalloc(page_table, 0, 4096)`。
7. `uvmalloc` 调用 `kalloc`，空闲物理页数量从 256 变为 255。
8. `mappages` 建立“虚拟页 0 → 新物理页”的映射。
9. `growproc` 将进程大小更新为 4096。
10. `sim_sbrk` 返回 `heap + 0`，即新增虚拟堆区间的起点。
11. `morecore` 在区间开头写入 `BlockHeader`，再调用 `my_free` 将整页加入空闲链表。
12. `my_malloc` 从该空闲块末尾切出 128B，返回跳过块头后的地址。

后续小分配会继续使用该页剩余空间。只有用户态空闲链表不足时，才再次执行 `morecore → sim_sbrk → growproc`。

## 释放路径

`my_free` 和 `kfree` 处于不同层：

```text
my_free：
将小内存块还给进程自己的 malloc 空闲链表
→ 不减少 ProcessMemory::size
→ 不归还物理页

uvmdealloc：
删除整页虚拟地址映射
→ 调用 kfree
→ 将物理页还给物理页分配器
```

因此，调用 `my_free` 后物理空闲页数量不一定增加。这与真实进程释放小块后，内存通常仍保留在用户态分配器中一致。

## 与 xv6 的对应关系

| 本项目 | xv6 位置或概念 |
|---|---|
| `my_malloc / my_free` | `user/umalloc.c` |
| `morecore` | `user/umalloc.c` 中的 `morecore` |
| `sim_sbrk` | 用户态 `sbrk` 和系统调用边界的简化模拟 |
| `growproc` | `kernel/proc.c` 中的 `growproc` |
| `uvmalloc / uvmdealloc / mappages` | `kernel/vm.c` |
| `kinit / kalloc / kfree` | `kernel/kalloc.c` |

## 编译和运行

在项目目录执行：

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -g -O0 -Iinclude src/page_allocator.cpp src/virtual_memory.cpp src/process_memory.cpp src/mini_malloc.cpp tests/test_mini_malloc.cpp -o tests/test_mini_malloc.exe
./tests/test_mini_malloc.exe
```

当前正式测试覆盖：

- `my_malloc(0)` 返回 `nullptr`。
- 第一次分配返回非空且 16B 对齐的指针。

开发过程中还验证了跨页分配、1MB 内存耗尽、失败回滚、全部释放后的大块复用，以及三个调试校验函数。

## 项目边界

- 项目只有一个模拟进程和 1MB 虚拟地址空间。
- 物理内存固定为 1MB，共 256 页。
- 代码按单线程运行，不维护自旋锁。
- `my_free` 不主动缩小进程地址空间。
- 页表使用数组模拟，不是 RISC-V 三级页表。
- `heap` 是实际返回给 C++ 调用者的连续缓冲区；`PageTable` 和 `physical_memory` 模拟分配与映射元数据，CPU 不会使用它们完成真实地址翻译。

这个项目的重点是理解各层职责和控制流，而不是替代标准库分配器或操作系统虚拟内存实现。
