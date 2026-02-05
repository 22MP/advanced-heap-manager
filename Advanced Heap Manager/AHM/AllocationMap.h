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

        // initialCapacityPow2 is total capacity across all stripes (must be pow2)
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
            void* key;    // ptr
            uint64_t hash;   // full hash (stored to speed comparisons & rehash)
            uint32_t value;  // heapId
            State    state;  // 0-inited -> Empty
        };

        struct Segment
        {
            SRWLOCK _lock{};
            Entry* _entries = nullptr;
            size_t  _capacity = 0;    // pow2
            size_t  _size = 0;
            size_t  _tombstones = 0;
        };

    private:
        static bool     IsPowerOfTwo(size_t x);
        static uint64_t HashPtr(void* p);

        // Stripe config
        static constexpr size_t kSegmentCount = 64; // must be pow2
        static constexpr size_t kSegmentMask = kSegmentCount - 1;

        // segment index from hash (capacity-independent)
        static size_t SegmentIndex(uint64_t hash) { return static_cast<size_t>(hash) & kSegmentMask; }

        // index seed inside a segment
        static size_t SegmentStart(uint64_t hash, size_t segMask)
        {
            // shift away the segment-selection bits
            constexpr int kShift = 6; // log2(64)
            return static_cast<size_t>(hash >> kShift) & segMask;
        }

        bool InitSegmentNoLock(Segment& s, size_t capPow2);
        void DestroySegmentNoLock(Segment& s);

        bool EnsureCapacityNoLock(Segment& s);
        bool PutNoLock(Segment& s, void* ptr, uint32_t heapId, uint64_t hash);
        bool LookupNoLock(Segment& s, void* ptr, uint64_t hash, size_t& indexOut, uint32_t& heapIdOut);
        void RemoveAtNoLock(Segment& s, size_t index);

        bool ShouldRebuildNoLock(const Segment& s) const;
        bool RebuildSameCapacityNoLock(Segment& s);

    private:
        Segment _segs[kSegmentCount];
        bool    _initialized = false;
    };
}
