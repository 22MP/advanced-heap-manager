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
        enum class State : uint8_t { Empty = 0, Occupied = 1, Tombstone = 2 };

        struct Entry
        {
            void* key;     // pointer key
            uint64_t hash;    // cached hash of key
            uint32_t value;   // heapId
            State    state;   // Empty/Occupied/Tombstone
        };

        SRWLOCK _lock;

        Entry* _entries;
        size_t _capacity;
        size_t _size;
        size_t _tombstones;

        static bool     IsPowerOfTwo(size_t x);
        static uint64_t HashPtr(void* p);

        // Caller must hold exclusive lock:
        bool EnsureCapacityNoLock();

        bool PutNoLock(void* ptr, uint32_t heapId, uint64_t hash);

        bool LookupNoLock(void* ptr, uint64_t hash, size_t& indexOut, uint32_t& heapIdOut);
        void RemoveAtNoLock(size_t index);

        bool ShouldRebuildNoLock() const;
        bool RebuildSameCapacityNoLock();
    };
}
