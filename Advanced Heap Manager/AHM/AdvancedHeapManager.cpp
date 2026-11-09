#include "pch.h"
#include "AdvancedHeapManager.h"

#include "AllocationHeader.h"
#include "AllocationMap.h"
#include "HeapEntry.h"

#include <atomic>
#include <cstdint>

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

        // Allocation strategy (map or header)
        Strategy g_strategy = Strategy::Map;

        // Init/Shutdown guard
        SRWLOCK   g_stateLock;
        INIT_ONCE g_stateInitOnce = INIT_ONCE_STATIC_INIT;
        bool      g_initialized = false;

        static BOOL CALLBACK InitStateLockOnce(PINIT_ONCE, PVOID, PVOID*)
        {
            InitializeSRWLock(&g_stateLock);
            return TRUE;
        }

        static void EnsureStateLockInit()
        {
            InitOnceExecuteOnce(&g_stateInitOnce, InitStateLockOnce, nullptr, nullptr);
        }

        static bool IsReady()
        {
            return g_initialized && g_heaps && (g_heapCount != 0);
        }

        static size_t NextPow2(size_t x)
        {
            if (x == 0) return 1;
            --x;
            for (size_t shift = 1; shift < sizeof(size_t) * 8; shift <<= 1)
                x |= (x >> shift);
            return x + 1;
        }

        static void Shutdown_NoLock()
        {
            if (!g_initialized)
                return;

            // Destroy the allocation map
            g_map.Destroy();

            // Destroy heaps
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
            g_rr.store(0);
            g_initialized = false;
        }
    }

    // ---------- Strategy 1: AllocationMap ----------
    static void* MallocMap(size_t sizeBytes)
    {
        const uint32_t ticket = g_rr.fetch_add(1, std::memory_order_relaxed);
        const uint32_t heapId =
            static_cast<uint32_t>(ticket % static_cast<uint32_t>(g_heapCount));

        HeapEntry& he = g_heaps[heapId];

        AcquireSRWLockExclusive(&he.heapLock);
        void* ptr = HeapAlloc(he.heapHandle, 0, sizeBytes);
        ReleaseSRWLockExclusive(&he.heapLock);

        if (!ptr)
            return nullptr;

        if (!g_map.Put(ptr, heapId))
        {
            AcquireSRWLockExclusive(&he.heapLock);
            HeapFree(he.heapHandle, 0, ptr);
            ReleaseSRWLockExclusive(&he.heapLock);
            return nullptr;
        }
        return ptr;
    }

    static void FreeMap(void* ptr)
    {
        uint32_t heapId = 0;
        if (!g_map.Take(ptr, heapId))
            return;
        if (heapId >= g_heapCount)
            return;

        HeapEntry& he = g_heaps[heapId];

        AcquireSRWLockExclusive(&he.heapLock);
        HeapFree(he.heapHandle, 0, ptr);
        ReleaseSRWLockExclusive(&he.heapLock);
    }

    // ---------- Strategy 2: AllocationHeader ----------
    static void* MallocHeader(size_t sizeBytes)
    {
        // Protect against integer overflow when calculating the total allocation size
        if (sizeBytes > SIZE_MAX - sizeof(AllocationHeader))
            return nullptr;
        const size_t totalSize = sizeof(AllocationHeader) + sizeBytes;

        const uint32_t ticket = g_rr.fetch_add(1, std::memory_order_relaxed);
        const uint32_t heapId =
            static_cast<uint32_t>(ticket % static_cast<uint32_t>(g_heapCount));

        HeapEntry& he = g_heaps[heapId];

        AcquireSRWLockExclusive(&he.heapLock);
        void* raw = HeapAlloc(he.heapHandle, 0, totalSize);
        ReleaseSRWLockExclusive(&he.heapLock);

        if (!raw)
            return nullptr;

        AllocationHeader* hdr = static_cast<AllocationHeader*>(raw);
        hdr->heapId = heapId;
        hdr->magic = AHM_MAGIC;

        return hdr + 1;    // The user receives the address immediately after the header
    }

    static void FreeHeader(void* ptr)
    {
        AllocationHeader* hdr = static_cast<AllocationHeader*>(ptr) - 1;

        if (hdr->magic != AHM_MAGIC)
            return;                       // Not our block or it has already been freed

        const uint32_t heapId = hdr->heapId;
        if (heapId >= g_heapCount)
            return;

        hdr->magic = 0;                   // Mark the block as freed to detect subsequent Free() calls

        HeapEntry& he = g_heaps[heapId];

        AcquireSRWLockExclusive(&he.heapLock);
        HeapFree(he.heapHandle, 0, hdr);  // Free the beginning of the block (hdr), not ptr
        ReleaseSRWLockExclusive(&he.heapLock);
    }

    void Initialize(size_t heapCount, size_t heapSizeBytes, Strategy strategy)
    {
        EnsureStateLockInit();
        AcquireSRWLockExclusive(&g_stateLock);

        // Perform a hard reset if already initialized
        if (g_initialized)
        {
            Shutdown_NoLock();
        }

        if (heapCount == 0 || heapSizeBytes == 0)
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

            // Initial size = heapSizeBytes, maximum size = 0 (grows as needed)
            g_heaps[i].heapHandle = HeapCreate(HEAP_NO_SERIALIZE, heapSizeBytes, 0);

            if (!g_heaps[i].heapHandle)
            {
                // Roll back all heaps that were successfully created
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

        // Create the allocation map only when using the Map strategy
        if (strategy == Strategy::Map)
        {
            size_t desired = heapCount * 1024;
            if (desired < 1024) desired = 1024;
            const size_t cap = NextPow2(desired);

            if (!g_map.Init(cap))
            {
                g_map.Destroy();

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
        }

        g_strategy = strategy;
        g_rr.store(0, std::memory_order_relaxed);
        g_initialized = true;

        ReleaseSRWLockExclusive(&g_stateLock);
    }

    void Shutdown()
    {
        EnsureStateLockInit();
        AcquireSRWLockExclusive(&g_stateLock);

        Shutdown_NoLock();

        ReleaseSRWLockExclusive(&g_stateLock);
    }

    void* Malloc(size_t sizeBytes)
    {
        if (sizeBytes == 0 || !IsReady())
            return nullptr;

        return (g_strategy == Strategy::Header) ? MallocHeader(sizeBytes)
            : MallocMap(sizeBytes);
    }

    void Free(void* ptr)
    {
        if (!ptr || !IsReady())
            return;

        if (g_strategy == Strategy::Header) FreeHeader(ptr);
        else                                FreeMap(ptr);
    }
}