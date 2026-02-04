#pragma once
#include <cstddef>
#include <cstdint>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace AHM
{
    class AllocationMap
    {
    public:
        AllocationMap();
        ~AllocationMap();

        bool Init(size_t initialCapacityPow2);
        void Destroy();

        bool Put(void* ptr, uint32_t heapId);
        bool Take(void* ptr, uint32_t& heapIdOut);

        AllocationMap(const AllocationMap&) = delete;
        AllocationMap& operator=(const AllocationMap&) = delete;

    private:
        enum class State : uint8_t { Empty, Occupied, Tombstone };

        struct Entry
        {
            void* key;
            uint32_t value;
            State    state;
        };

        SRWLOCK _lock;

        Entry* _entries;
        size_t _capacity;
        size_t _size;
        size_t _tombstones;

        static bool     IsPowerOfTwo(size_t x);
        static uint64_t HashPtr(void* p);

        bool EnsureCapacityNoLock();
        bool PutNoLock(void* ptr, uint32_t heapId);
        bool TakeNoLock(void* ptr, uint32_t& heapIdOut);
    };
}