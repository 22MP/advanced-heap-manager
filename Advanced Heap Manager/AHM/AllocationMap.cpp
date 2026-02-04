#include "pch.h"
#include "AllocationMap.h"
#include <cstdlib>

namespace AHM
{
    AllocationMap::AllocationMap()
        : _entries(nullptr)
        , _capacity(0)
        , _size(0)
        , _tombstones(0)
    {
        InitializeSRWLock(&_lock);
    }

    AllocationMap::~AllocationMap()
    {
        Destroy();
    }

    bool AllocationMap::Init(size_t initialCapacityPow2)
    {
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

        // HEAP_ZERO_MEMORY => state==0 => State::Empty
        return true;
    }

    void AllocationMap::Destroy()
    {
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
        if (!ptr)
            return false;

        // Non-critical work outside lock
        const uint64_t hash = HashPtr(ptr);

        AcquireSRWLockExclusive(&_lock);

        if (!EnsureCapacityNoLock())
        {
            ReleaseSRWLockExclusive(&_lock);
            return false;
        }

        const bool ok = PutNoLock(ptr, heapId, hash);

        ReleaseSRWLockExclusive(&_lock);
        return ok;
    }

    bool AllocationMap::Take(void* ptr, uint32_t& heapIdOut)
    {
        if (!ptr)
            return false;

        // Non-critical work outside lock
        const uint64_t hash = HashPtr(ptr);

        AcquireSRWLockExclusive(&_lock);

        size_t index = 0;
        uint32_t heapId = 0;

        if (!LookupNoLock(ptr, hash, index, heapId))
        {
            ReleaseSRWLockExclusive(&_lock);
            return false;
        }

        RemoveAtNoLock(index);
        heapIdOut = heapId;

        // Tombstone cleanup rebuild (best-effort)
        if (ShouldRebuildNoLock())
        {
            (void)RebuildSameCapacityNoLock();
        }

        ReleaseSRWLockExclusive(&_lock);
        return true;
    }

    // ---------------- Helpers ----------------

    bool AllocationMap::IsPowerOfTwo(size_t x)
    {
        return x != 0 && ((x & (x - 1)) == 0);
    }

    uint64_t AllocationMap::HashPtr(void* p)
    {
        uintptr_t castedPtr = reinterpret_cast<uintptr_t>(p);
        castedPtr >>= 4; // drop low alignment bits

        uint64_t x = static_cast<uint64_t>(castedPtr);

        // SplitMix64-style mixing
        x += 0x9E3779B97F4A7C15ULL;
        x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
        x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
        x = x ^ (x >> 31);

        return x;
    }

    bool AllocationMap::EnsureCapacityNoLock()
    {
        if (!_entries || _capacity == 0)
            return false;

        // Grow when (size + tombstones + 1) / capacity >= ~0.70
        // (Counts tombstones because they lengthen probes under churn.)
        const size_t needed = _size + _tombstones + 1;
        const bool shouldGrow = (needed * 100) >= (_capacity * 70);

        if (!shouldGrow)
            return true;

        const size_t newCap = _capacity * 2;
        if (newCap < _capacity) // overflow
            return false;

        Entry* newEntries = static_cast<Entry*>(
            HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, newCap * sizeof(Entry))
            );
        if (!newEntries)
            return false;

        const size_t newMask = newCap - 1;

        // Rehash occupied entries into the new table (tombstones are dropped)
        for (size_t i = 0; i < _capacity; ++i)
        {
            const Entry& oldE = _entries[i];
            if (oldE.state != State::Occupied)
                continue;

            const uint64_t h = oldE.hash; // USE STORED HASH
            const size_t start = static_cast<size_t>(h) & newMask;

            for (size_t step = 0; step < newCap; ++step)
            {
                const size_t idx = (start + step) & newMask;
                Entry& dst = newEntries[idx];

                if (dst.state == State::Empty)
                {
                    dst.key = oldE.key;
                    dst.hash = oldE.hash;
                    dst.value = oldE.value;
                    dst.state = State::Occupied;
                    break;
                }
            }
        }

        HeapFree(GetProcessHeap(), 0, _entries);
        _entries = newEntries;
        _capacity = newCap;
        _tombstones = 0; // tombstones removed by rehash
        // _size unchanged

        return true;
    }

    bool AllocationMap::PutNoLock(void* ptr, uint32_t heapId, uint64_t hash)
    {
        if (!_entries || _capacity == 0)
            return false;

        const size_t mask = _capacity - 1;
        const size_t start = static_cast<size_t>(hash) & mask;

        size_t firstTomb = static_cast<size_t>(-1);

        for (size_t step = 0; step < _capacity; ++step)
        {
            const size_t idx = (start + step) & mask;
            Entry& e = _entries[idx];

            if (e.state == State::Empty)
            {
                const size_t ins = (firstTomb != static_cast<size_t>(-1)) ? firstTomb : idx;
                Entry& dst = _entries[ins];

                dst.key = ptr;
                dst.hash = hash;
                dst.value = heapId;
                dst.state = State::Occupied;

                if (ins == firstTomb && _tombstones > 0)
                    --_tombstones;

                ++_size;
                return true;
            }

            if (e.state == State::Tombstone)
            {
                if (firstTomb == static_cast<size_t>(-1))
                    firstTomb = idx;
                continue;
            }

            // Occupied: quick reject by hash first, then key compare
            if (e.hash == hash && e.key == ptr)
            {
                e.value = heapId; // update
                return true;
            }
        }

        // No empty slot, but tombstone exists
        if (firstTomb != static_cast<size_t>(-1))
        {
            Entry& dst = _entries[firstTomb];

            dst.key = ptr;
            dst.hash = hash;
            dst.value = heapId;
            dst.state = State::Occupied;

            if (_tombstones > 0)
                --_tombstones;

            ++_size;
            return true;
        }

        return false;
    }

    bool AllocationMap::LookupNoLock(void* ptr, uint64_t hash, size_t& indexOut, uint32_t& heapIdOut)
    {
        if (!ptr || !_entries || _capacity == 0)
            return false;

        const size_t mask = _capacity - 1;
        const size_t start = static_cast<size_t>(hash) & mask;

        for (size_t step = 0; step < _capacity; ++step)
        {
            const size_t idx = (start + step) & mask;
            Entry& e = _entries[idx];

            if (e.state == State::Empty)
                return false; // stop: key not present

            if (e.state == State::Occupied)
            {
                if (e.hash == hash && e.key == ptr)
                {
                    indexOut = idx;
                    heapIdOut = e.value;
                    return true;
                }
            }
        }

        return false;
    }

    void AllocationMap::RemoveAtNoLock(size_t index)
    {
        Entry& e = _entries[index];

        e.key = nullptr;
        e.hash = 0;
        e.value = 0;
        e.state = State::Tombstone;

        if (_size > 0)
            --_size;

        ++_tombstones;
    }

    bool AllocationMap::ShouldRebuildNoLock() const
    {
        if (_capacity == 0)
            return false;

        // Rebuild if tombstones are a significant fraction, or more than live entries. Must be bigger that threshold.
        if (_tombstones < 4096)
            return false;

        if ((_tombstones * 100) >= (_capacity * 25))
            return true;

        if (_tombstones > _size)
            return true;

        const size_t unusable = _size + _tombstones;
        if ((unusable * 100) >= (_capacity * 85))
            return true;

        return false;
    }

    bool AllocationMap::RebuildSameCapacityNoLock()
    {
        if (!_entries || _capacity == 0)
            return false;

        const size_t newCap = _capacity;

        Entry* newEntries = static_cast<Entry*>(
            HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, newCap * sizeof(Entry))
            );
        if (!newEntries)
            return false;

        const size_t newMask = newCap - 1;

        for (size_t i = 0; i < _capacity; ++i)
        {
            const Entry& oldE = _entries[i];
            if (oldE.state != State::Occupied)
                continue;

            const uint64_t h = oldE.hash; // USE STORED HASH
            const size_t start = static_cast<size_t>(h) & newMask;

            for (size_t step = 0; step < newCap; ++step)
            {
                const size_t idx = (start + step) & newMask;
                Entry& dst = newEntries[idx];

                if (dst.state == State::Empty)
                {
                    dst.key = oldE.key;
                    dst.hash = oldE.hash;
                    dst.value = oldE.value;
                    dst.state = State::Occupied;
                    break;
                }
            }
        }

        HeapFree(GetProcessHeap(), 0, _entries);
        _entries = newEntries;

        // Capacity unchanged, live entries unchanged
        _tombstones = 0;

        return true;
    }
}
