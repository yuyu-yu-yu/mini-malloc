#pragma once

#include <cstddef>

// Allocate size bytes from the project-owned heap.
// Returns nullptr when size is 0 or there is no usable block.
void* my_malloc(std::size_t size);

// Free a pointer returned by my_malloc.
// my_free(nullptr) should do nothing.
void my_free(void* ptr);

// Debug helpers. Use these while developing and in tests.
bool validate_heap();
void dump_heap();
