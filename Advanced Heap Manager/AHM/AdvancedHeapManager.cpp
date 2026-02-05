#include "pch.h"
#include "AdvancedHeapManager.h"

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

            // Destroy map 
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
            g_rr.store(0, std::memory_order_relaxed);
            g_initialized = false;
        }
    }

    void Initialize(size_t heapCount, size_t heapSizeBytes)
    {
        EnsureStateLockInit();
        AcquireSRWLockExclusive(&g_stateLock);

        // Hard reset if already initialized
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

            // initial size = heapSizeBytes, max size = 0 (grow as needed)
            g_heaps[i].heapHandle = HeapCreate(HEAP_NO_SERIALIZE, heapSizeBytes, 0);

            if (!g_heaps[i].heapHandle)
            {
                // Roll back heaps already created
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

        // Init map (capacity as power of two for fast search)
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

        const uint32_t ticket = g_rr.fetch_add(1, std::memory_order_relaxed);
        const uint32_t heapId =
            static_cast<uint32_t>(ticket % static_cast<uint32_t>(g_heapCount));

        HeapEntry& he = g_heaps[heapId];


        AcquireSRWLockExclusive(&he.heapLock);
        void* ptr = HeapAlloc(he.heapHandle, 0, sizeBytes);
        ReleaseSRWLockExclusive(&he.heapLock);

        if (!ptr)
            return nullptr;

        // If map insert fails, rollback allocation.
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
        if (!ptr || !IsReady())
            return;

        uint32_t heapId = 0;
        if (!g_map.Take(ptr, heapId))
        {
            return;
        }

        if (heapId >= g_heapCount)
        {
            return;
        }

        HeapEntry& he = g_heaps[heapId];


        AcquireSRWLockExclusive(&he.heapLock);
        HeapFree(he.heapHandle, 0, ptr);
        ReleaseSRWLockExclusive(&he.heapLock);
    }
}
