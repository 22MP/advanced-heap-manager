#include "pch.h"
#include "AllocationMap.h"

namespace AHM
{
    // ------------------------------------------------------------
    // Constructor
    // Initializes SRW locks for each segment.
    // Segments are independently locked to reduce contention.
    // ------------------------------------------------------------
    AllocationMap::AllocationMap()
    {
        for (size_t i = 0; i < kSegmentCount; ++i)
            InitializeSRWLock(&_segs[i]._lock);
    }

    AllocationMap::~AllocationMap()
    {
        Destroy();
    }

    // ------------------------------------------------------------
    // Init
    // Initializes all segments with equal capacity.
    // initialCapacityPow2 must be power-of-two and is split per segment.
    // ------------------------------------------------------------
    bool AllocationMap::Init(size_t initialCapacityPow2)
    {
        if (!IsPowerOfTwo(initialCapacityPow2))
            return false;

        Destroy();

        // Divide capacity across segments
        size_t perSeg = initialCapacityPow2 / kSegmentCount;

        // Enforce minimum size to keep probing efficient
        if (perSeg < 8) perSeg = 8;

        // Round segment capacity up to next power of two
        auto nextPow2 = [](size_t x) -> size_t {
            if (x == 0) return 1;
            --x;
            for (size_t sh = 1; sh < sizeof(size_t) * 8; sh <<= 1)
                x |= (x >> sh);
            return x + 1;
            };
        perSeg = nextPow2(perSeg);

        // Initialize each segment independently
        for (size_t i = 0; i < kSegmentCount; ++i)
        {
            AcquireSRWLockExclusive(&_segs[i]._lock);
            const bool ok = InitSegmentNoLock(_segs[i], perSeg);
            ReleaseSRWLockExclusive(&_segs[i]._lock);

            if (!ok)
            {
                Destroy();
                return false;
            }
        }

        _initialized = true;
        return true;
    }

    // ------------------------------------------------------------
    // Destroy
    // Releases memory for all segments.
    // Safe to call multiple times.
    // ------------------------------------------------------------
    void AllocationMap::Destroy()
    {
        for (size_t i = 0; i < kSegmentCount; ++i)
        {
            AcquireSRWLockExclusive(&_segs[i]._lock);
            DestroySegmentNoLock(_segs[i]);
            ReleaseSRWLockExclusive(&_segs[i]._lock);
        }
        _initialized = false;
    }

    // ------------------------------------------------------------
    // Put
    // Inserts or updates ptr -> heapId mapping.
    // Segment is chosen via hashed pointer.
    // ------------------------------------------------------------
    bool AllocationMap::Put(void* ptr, uint32_t heapId)
    {
        if (!_initialized || !ptr)
            return false;

        const uint64_t hash = HashPtr(ptr);
        Segment& s = _segs[SegmentIndex(hash)];

        AcquireSRWLockExclusive(&s._lock);

        // Ensure capacity before inserting
        if (!EnsureCapacityNoLock(s))
        {
            ReleaseSRWLockExclusive(&s._lock);
            return false;
        }

        const bool ok = PutNoLock(s, ptr, heapId, hash);

        ReleaseSRWLockExclusive(&s._lock);
        return ok;
    }

    // ------------------------------------------------------------
    // Take
    // Removes ptr mapping and returns heapId.
    // May trigger rebuild if tombstones accumulate.
    // ------------------------------------------------------------
    bool AllocationMap::Take(void* ptr, uint32_t& heapIdOut)
    {
        if (!_initialized || !ptr)
            return false;

        const uint64_t hash = HashPtr(ptr);
        Segment& s = _segs[SegmentIndex(hash)];

        size_t index = 0;
        uint32_t heapId = 0;
        
        AcquireSRWLockExclusive(&s._lock);
        // Lookup entry
        if (!LookupNoLock(s, ptr, hash, index, heapId))
        {
            ReleaseSRWLockExclusive(&s._lock);
            return false;
        }

        // Mark slot as tombstone
        RemoveAtNoLock(s, index);
        heapIdOut = heapId;

        // Rebuild segment if tombstones degrade performance
        if (ShouldRebuildNoLock(s))
        {
            (void)RebuildSameCapacityNoLock(s);
        }

        ReleaseSRWLockExclusive(&s._lock);
        return true;
    }

    // ==================== Helpers ====================

    // ------------------------------------------------------------
    // IsPowerOfTwo
    // Used to enforce mask-based indexing.
    // ------------------------------------------------------------
    bool AllocationMap::IsPowerOfTwo(size_t x)
    {
        return x != 0 && ((x & (x - 1)) == 0);
    }

    // ------------------------------------------------------------
    // HashPtr
    // Hashes pointer value with alignment stripping and mix.
    // Lower bits are ignored to avoid allocator alignment patterns.
    // ------------------------------------------------------------
    uint64_t AllocationMap::HashPtr(void* p)
    {
        uintptr_t castedPtr = reinterpret_cast<uintptr_t>(p);
        castedPtr >>= 4; // drop alignment bits

        uint64_t x = static_cast<uint64_t>(castedPtr);

        // SplitMix64-style avalanche
        x += 0x9E3779B97F4A7C15ULL;
        x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
        x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
        x = x ^ (x >> 31);

        return x;
    }

    // ------------------------------------------------------------
    // InitSegmentNoLock
    // Allocates and initializes segment storage.
    // Caller must hold segment lock.
    // ------------------------------------------------------------
    bool AllocationMap::InitSegmentNoLock(Segment& s, size_t capPow2)
    {
        DestroySegmentNoLock(s);

        s._entries = static_cast<Entry*>(
            HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, capPow2 * sizeof(Entry))
            );
        if (!s._entries)
            return false;

        s._capacity = capPow2;
        s._size = 0;
        s._tombstones = 0;
        return true;
    }

    // ------------------------------------------------------------
    // DestroySegmentNoLock
    // Frees segment storage and resets counters.
    // ------------------------------------------------------------
    void AllocationMap::DestroySegmentNoLock(Segment& s)
    {
        if (s._entries)
        {
            HeapFree(GetProcessHeap(), 0, s._entries);
            s._entries = nullptr;
        }
        s._capacity = 0;
        s._size = 0;
        s._tombstones = 0;
    }

    // ------------------------------------------------------------
    // EnsureCapacityNoLock
    // Grows segment when load factor exceeds threshold.
    // Tombstones are counted as occupied.
    // ------------------------------------------------------------
    bool AllocationMap::EnsureCapacityNoLock(Segment& s)
    {
        if (!s._entries || s._capacity == 0)
            return false;

        const size_t needed = s._size + s._tombstones + 1;
        const bool shouldGrow = (needed * 100) >= (s._capacity * 85);

        if (!shouldGrow)
            return true;

        const size_t newCap = s._capacity * 2;
        if (newCap < s._capacity)
            return false; // overflow

        Entry* newEntries = static_cast<Entry*>(
            HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, newCap * sizeof(Entry))
            );
        if (!newEntries)
            return false;

        const size_t newMask = newCap - 1;
        size_t newSize = 0;


        // Rehash occupied entries
        for (size_t i = 0; i < s._capacity; ++i)
        {
            const Entry& oldE = s._entries[i];
            if (oldE.state != State::Occupied)
                continue;

            const uint64_t h = oldE.hash;
            const size_t start = SegmentStart(h, newMask);

            for (size_t step = 0; step < newCap; ++step)
            {
                const size_t idx = (start + step) & newMask;
                Entry& dst = newEntries[idx];

                if (dst.state == State::Empty)
                {
                    ++newSize;
                    dst.key = oldE.key;
                    dst.hash = oldE.hash;
                    dst.value = oldE.value;
                    dst.state = State::Occupied;
                    break;
                }
            }
        }

        HeapFree(GetProcessHeap(), 0, s._entries);
        s._entries = newEntries;
        s._capacity = newCap;
        s._size = newSize;
        s._tombstones = 0;

        return true;
    }

    // ------------------------------------------------------------
    // PutNoLock
    // Linear probing insert with tombstone reuse.
    // ------------------------------------------------------------
    bool AllocationMap::PutNoLock(Segment& s, void* ptr, uint32_t heapId, uint64_t hash)
    {
        if (!s._entries || s._capacity == 0)
            return false;

        const size_t mask = s._capacity - 1;
        const size_t start = SegmentStart(hash, mask);

        size_t firstTomb = static_cast<size_t>(-1);

        for (size_t step = 0; step < s._capacity; ++step)
        {
            const size_t idx = (start + step) & mask;
            Entry& e = s._entries[idx];

            if (e.state == State::Empty)
            {
                const size_t ins = (firstTomb != static_cast<size_t>(-1)) ? firstTomb : idx;
                Entry& dst = s._entries[ins];

                dst.key = ptr;
                dst.hash = hash;
                dst.value = heapId;
                dst.state = State::Occupied;

                if (ins == firstTomb && s._tombstones > 0)
                    --s._tombstones;

                ++s._size;
                return true;
            }

            if (e.state == State::Tombstone)
            {
                if (firstTomb == static_cast<size_t>(-1))
                    firstTomb = idx;
                continue;
            }

            // Update existing entry
            if (e.hash == hash && e.key == ptr)
            {
                e.value = heapId;
                return true;
            }
        }

        // Insert into first tombstone if no empty slot found
        if (firstTomb != static_cast<size_t>(-1))
        {
            Entry& dst = s._entries[firstTomb];

            dst.key = ptr;
            dst.hash = hash;
            dst.value = heapId;
            dst.state = State::Occupied;

            if (s._tombstones > 0)
                --s._tombstones;

            ++s._size;
            return true;
        }

        return false;
    }

    // ------------------------------------------------------------
    // LookupNoLock
    // Linear probing lookup.
    // Stops early on empty slot.
    // ------------------------------------------------------------
    bool AllocationMap::LookupNoLock(Segment& s, void* ptr, uint64_t hash, size_t& indexOut, uint32_t& heapIdOut)
    {
        if (!ptr || !s._entries || s._capacity == 0)
            return false;

        const size_t mask = s._capacity - 1;
        const size_t start = SegmentStart(hash, mask);

        for (size_t step = 0; step < s._capacity; ++step)
        {
            const size_t idx = (start + step) & mask;
            Entry& e = s._entries[idx];

            if (e.state == State::Empty)
                return false;

            if (e.state == State::Occupied &&
                e.hash == hash && e.key == ptr)
            {
                indexOut = idx;
                heapIdOut = e.value;
                return true;
            }
        }

        return false;
    }

    // ------------------------------------------------------------
    // RemoveAtNoLock
    // Converts occupied slot into tombstone.
    // ------------------------------------------------------------
    void AllocationMap::RemoveAtNoLock(Segment& s, size_t index)
    {
        Entry& e = s._entries[index];

        e.key = nullptr;
        e.hash = 0;
        e.value = 0;
        e.state = State::Tombstone;

        if (s._size > 0)
            --s._size;

        ++s._tombstones;
    }

    // ------------------------------------------------------------
    // ShouldRebuildNoLock
    // Heuristic-based rebuild trigger.
    // ------------------------------------------------------------
    bool AllocationMap::ShouldRebuildNoLock(const Segment& s) const
    {
        if (s._capacity == 0)
            return false;

        if (s._tombstones < 1024)
            return false;

        if ((s._tombstones * 100) >= (s._capacity * 25))
            return true;

        if (s._tombstones > s._size)
            return true;

        const size_t unusable = s._size + s._tombstones;
        if ((unusable * 100) >= (s._capacity * 85))
            return true;

        return false;
    }

    // ------------------------------------------------------------
    // RebuildSameCapacityNoLock
    // Rehashes occupied entries into fresh table of same size.
    // Removes all tombstones.
    // ------------------------------------------------------------
    bool AllocationMap::RebuildSameCapacityNoLock(Segment& s)
    {
        if (!s._entries || s._capacity == 0)
            return false;

        const size_t newCap = s._capacity;

        Entry* newEntries = static_cast<Entry*>(
            HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, newCap * sizeof(Entry))
            );
        if (!newEntries)
            return false;

        const size_t newMask = newCap - 1;
        size_t newSize = 0;
        for (size_t i = 0; i < s._capacity; ++i)
        {
            const Entry& oldE = s._entries[i];
            if (oldE.state != State::Occupied)
                continue;

            const uint64_t h = oldE.hash;
            const size_t start = SegmentStart(h, newMask);

            for (size_t step = 0; step < newCap; ++step)
            {
                const size_t idx = (start + step) & newMask;
                Entry& dst = newEntries[idx];

                if (dst.state == State::Empty)
                {
                    ++newSize;
                    dst.key = oldE.key;
                    dst.hash = oldE.hash;
                    dst.value = oldE.value;
                    dst.state = State::Occupied;
                    break;
                }
            }
        }

        HeapFree(GetProcessHeap(), 0, s._entries);
        s._entries = newEntries;
        s._size = newSize;
        s._tombstones = 0;

        return true;
    }
}
