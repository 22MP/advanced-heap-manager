#pragma once
#pragma once
#include <cstdint>

namespace AHM
{
    static constexpr uint32_t AHM_MAGIC = 0xABCDEF12;

    struct alignas(16) AllocationHeader
    {
        uint32_t heapId;   // ID of the heap the block belongs to
        uint32_t magic;    // AHM_MAGIC while the block is valid
    };

    static_assert(sizeof(AllocationHeader) == 16,
        "Header must be 16B to keep the user pointer aligned to 16 bytes");
}