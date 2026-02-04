#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace AHM
{
    struct HeapEntry
    {
        HANDLE  heapHandle;   // from HeapCreate
        SRWLOCK heapLock;     // per-heap lock 
    };
}