#include "pch.h"
#include "AdvancedHeapManager.h"

#include "AllocationMap.h"
#include "HeapEntry.h"

#include <atomic>
#include <cstdint>
#include <cassert>

namespace AHM
{
    namespace
    {
        // Global AHM state 
        HeapEntry* g_heaps = nullptr;
        size_t     g_heapCount = 0;

        // Round-robin counter
        std::atomic<uint32_t> g_rr{ 0 };

        // Pointer -> heapId tracking
        AllocationMap g_map;

        // Init/Shutdown guard 
        SRWLOCK g_stateLock;
        bool    g_initialized = false;
        bool    g_stateLockInit = false;

        static size_t NextPow2(size_t x)
        {
            if (x == 0) return 1; 
            --x;
            for (size_t shift = 1; shift < sizeof(size_t) * 8; shift <<= 1)
                x |= (x >> shift);
            return x + 1;
        }

        static void EnsureStateLockInit()
        {
            if (!g_stateLockInit)
            {
                InitializeSRWLock(&g_stateLock);
                g_stateLockInit = true;
            }
        }
    }

    void Initialize(size_t heapCount, size_t heapSizeBytes)
    {
        EnsureStateLockInit();

        AcquireSRWLockExclusive(&g_stateLock);

        // Allow re-init: shutdown any previous state first.
        if (g_initialized)
        {
            ReleaseSRWLockExclusive(&g_stateLock);
            Shutdown();
            AcquireSRWLockExclusive(&g_stateLock);
        }

        if (heapCount == 0)
        {
            ReleaseSRWLockExclusive(&g_stateLock);
            return;
        }


        if (heapCount > static_cast<size_t>(UINT32_MAX))
        {
            ReleaseSRWLockExclusive(&g_stateLock);
            return;
        }

        g_heaps = static_cast<HeapEntry*>(
            HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, heapCount * sizeof(HeapEntry))
            );

        if (!g_heaps)
        {
            ReleaseSRWLockExclusive(&g_stateLock);
            return;
        }

        g_heapCount = heapCount;

        // Create heaps
        for (size_t i = 0; i < g_heapCount; ++i)
        {
            InitializeSRWLock(&g_heaps[i].heapLock);

            // Use heapSizeBytes as initial commit; allow growth (max=0).
            g_heaps[i].heapHandle = HeapCreate(0, heapSizeBytes, 0);

            if (!g_heaps[i].heapHandle)
            {
                // Roll back everything if any heap fails.
                for (size_t j = 0; j < i; ++j)
                {
                    if (g_heaps[j].heapHandle)
                    {
                        HeapDestroy(g_heaps[j].heapHandle);
                        g_heaps[j].heapHandle = nullptr;
                    }
                }

                HeapFree(GetProcessHeap(), 0, g_heaps);
                g_heaps = nullptr;
                g_heapCount = 0;

                ReleaseSRWLockExclusive(&g_stateLock);
                return;
            }
        }

        // Map initial capacity: power-of-two, sized to reduce early rehashing.
        size_t desired = heapCount * 1024;
        if (desired < 1024) desired = 1024;

        const size_t cap = NextPow2(desired);

        if (!g_map.Init(cap))
        {
            // If the map can't initialize, destroy heaps and fail init.
            for (size_t i = 0; i < g_heapCount; ++i)
            {
                if (g_heaps[i].heapHandle)
                    HeapDestroy(g_heaps[i].heapHandle);
                g_heaps[i].heapHandle = nullptr;
            }

            HeapFree(GetProcessHeap(), 0, g_heaps);
            g_heaps = nullptr;
            g_heapCount = 0;

            ReleaseSRWLockExclusive(&g_stateLock);
            return;
        }

        g_rr.store(0, std::memory_order_relaxed);
        g_initialized = true;

        ReleaseSRWLockExclusive(&g_stateLock);
    }

    void Shutdown()
    {
        EnsureStateLockInit();

        AcquireSRWLockExclusive(&g_stateLock);

        if (!g_initialized)
        {
            ReleaseSRWLockExclusive(&g_stateLock);
            return;
        }

        // Assumption: caller already freed all allocations (as harness should).
        g_map.Destroy();

        if (g_heaps)
        {
            for (size_t i = 0; i < g_heapCount; ++i)
            {
                if (g_heaps[i].heapHandle)
                {
                    HeapDestroy(g_heaps[i].heapHandle);
                    g_heaps[i].heapHandle = nullptr;
                }
            }

            HeapFree(GetProcessHeap(), 0, g_heaps);
            g_heaps = nullptr;
        }

        g_heapCount = 0;
        g_rr.store(0, std::memory_order_relaxed);
        g_initialized = false;

        ReleaseSRWLockExclusive(&g_stateLock);
    }

    void* Malloc(size_t sizeBytes)
    {
        if (sizeBytes == 0)
            return nullptr;

        // Hot path: no global lock. Assume Initialize/Shutdown aren't concurrent with heavy alloc/free.
        if (!g_initialized || !g_heaps || g_heapCount == 0)
            return nullptr;

        const uint32_t ticket = g_rr.fetch_add(1, std::memory_order_relaxed);
        const uint32_t heapId = static_cast<uint32_t>(ticket % static_cast<uint32_t>(g_heapCount)); //num mod heapcount -> heap id

        HeapEntry& he = g_heaps[heapId];

        // Critical section: only around HeapAlloc on the chosen heap.
        AcquireSRWLockExclusive(&he.heapLock);
        void* ptr = HeapAlloc(he.heapHandle, 0, sizeBytes);
        ReleaseSRWLockExclusive(&he.heapLock);

        if (!ptr)
            return nullptr;

        // Record ownership. If the map insert fails, rollback allocation.
        if (!g_map.Put(ptr, heapId))
        {
            AcquireSRWLockExclusive(&he.heapLock);
            HeapFree(he.heapHandle, 0, ptr);
            ReleaseSRWLockExclusive(&he.heapLock);
            return nullptr;
        }

        return ptr;
    }

    void Free(void* ptr)
    {
        if (!ptr)
            return;

        if (!g_initialized || !g_heaps || g_heapCount == 0)
            return;

        uint32_t heapId = 0;
        if (!g_map.Take(ptr, heapId))
        {
            // Unknown pointer: never guess.
            assert(false && "AHM::Free received pointer not present in AllocationMap");
            return;
        }

        if (heapId >= g_heapCount)
        {
            assert(false && "AllocationMap returned invalid heapId");
            return;
        }

        HeapEntry& he = g_heaps[heapId];

        AcquireSRWLockExclusive(&he.heapLock);
        HeapFree(he.heapHandle, 0, ptr);
        ReleaseSRWLockExclusive(&he.heapLock);
    }
}
