#pragma once
#include <cstddef>

namespace AHM
{
    void Initialize(size_t heapCount, size_t heapSize);
    void Shutdown();

    void* Malloc(size_t size);
    void  Free(void* ptr);
}