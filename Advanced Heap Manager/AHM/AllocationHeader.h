#pragma once
#pragma once
#include <cstdint>

namespace AHM
{
    static constexpr uint32_t AHM_MAGIC = 0xABCDEF12;

    struct alignas(16) AllocationHeader
    {
        uint32_t heapId;   // iz kog heap-a je blok
        uint32_t magic;    // AHM_MAGIC dok je blok živ
        // alignas(16) dopuni strukturu na 16 bajtova
    };

    static_assert(sizeof(AllocationHeader) == 16,
        "Header mora biti 16B da korisnicki pointer ostane poravnat na 16");
}