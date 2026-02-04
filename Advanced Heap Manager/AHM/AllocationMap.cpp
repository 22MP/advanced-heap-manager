#include "pch.h"
#include "AllocationMap.h"
#include <cstdlib>   

namespace AHM
{
    AllocationMap::AllocationMap()
        : _entries(nullptr),
        _capacity(0),
        _size(0),
        _tombstones(0)
    {
        InitializeSRWLock(&_lock);
    }

    AllocationMap::~AllocationMap()
    {
        Destroy();
    }

    bool AllocationMap::Init(size_t initialCapacityPow2)
    {
        // Stub step: only allocate/reset storage and validate input.

        if (!IsPowerOfTwo(initialCapacityPow2))
            return false;

        Destroy();

        _entries = static_cast<Entry*>(
            HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, initialCapacityPow2 * sizeof(Entry))
            );

        if (!_entries)
            return false;

        _capacity = initialCapacityPow2;
        _size = 0;
        _tombstones = 0;

        // HEAP_ZERO_MEMORY already sets state=0 -> Empty
        return true;
    }

    void AllocationMap::Destroy()
    {
        // Safe to call multiple times
        if (_entries)
        {
            HeapFree(GetProcessHeap(), 0, _entries);
            _entries = nullptr;
        }

        _capacity = 0;
        _size = 0;
        _tombstones = 0;
    }

    bool AllocationMap::Put(void* ptr, uint32_t heapId)
    {
        // Stub: thread-safe wrapper + placeholder return.
        if (!ptr) return false;

        AcquireSRWLockExclusive(&_lock);
        const bool ok = PutNoLock(ptr, heapId);
        ReleaseSRWLockExclusive(&_lock);

        return ok;
    }

    bool AllocationMap::Take(void* ptr, uint32_t& heapIdOut)
    {
        // Stub: thread-safe wrapper + placeholder return.
        if (!ptr) return false;

        AcquireSRWLockExclusive(&_lock);
        const bool ok = TakeNoLock(ptr, heapIdOut);
        ReleaseSRWLockExclusive(&_lock);

        return ok;
    }

    // ---------------- Helpers (stubs for now) ----------------

    bool AllocationMap::IsPowerOfTwo(size_t x)
    {
        return x != 0 && ((x & (x - 1)) == 0);
    }

    uint64_t AllocationMap::HashPtr(void* p)
    {

        //convert ptr -> unsigned integer
        uintptr_t castedPtr = reinterpret_cast<uintptr_t>(p);

        //drop low alignment bits that are usually zero
        castedPtr >>= 4;

        //widen to 64-bit value for hashing
        uint64_t x = static_cast<uint64_t>(castedPtr);


        //bit mixing to improve hash distribution (SplitMix64-style)
        x += 0x9E3779B97F4A7C15ULL;
        x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
        x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
        x = x ^ (x >> 31);

        return x;
    }

    bool AllocationMap::EnsureCapacityNoLock()
    {
        // Stub: resizing will be implemented later
        return (_entries != nullptr && _capacity != 0);
    }

    bool AllocationMap::PutNoLock(void* /*ptr*/, uint32_t /*heapId*/)
    {
        // Stub: always fail until implemented
        return false;
    }

    bool AllocationMap::TakeNoLock(void* /*ptr*/, uint32_t& /*heapIdOut*/)
    {
        // Stub: always fail until implemented
        return false;
    }
}