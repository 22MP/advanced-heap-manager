#pragma once
#include <cstddef>

namespace AHM
{
    enum class Strategy
    {
        Map,     // ptr -> heapId through AllocationMap
        Header   // heapId in the 16B header preceding the block
    };

    void Initialize(size_t heapCount, size_t heapSizeBytes, Strategy strategy = Strategy::Map);
    void Shutdown();

    void* Malloc(size_t sizeBytes);
    void  Free(void* ptr);
}