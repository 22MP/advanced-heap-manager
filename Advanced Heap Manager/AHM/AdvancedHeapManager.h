#pragma once
#include <cstddef>

namespace AHM
{
    void Initialize(size_t heapCount, size_t heapSizeBytes);
    void Shutdown();

    void* Malloc(size_t sizeBytes);
    void  Free(void* ptr);
}