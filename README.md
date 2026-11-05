# Advanced Heap Manager (AHM)

AHM is a C++ memory manager for Windows that reduces **heap contention** in multithreaded programs. Instead of sending every allocation through one shared heap (and one lock), it spreads allocations over several independent Windows heaps in round-robin order and always returns memory to the heap it came from.

## The problem

A standard heap protects its internal structures with a global lock. Under heavy multithreaded allocation, threads queue up on that lock: allocations are effectively serialized, CPU time is spent waiting, and adding threads can make things worse instead of better. This is typical for servers that allocate temporary buffers per connection.

## How it works

- **Heap pool.** `Initialize(heapCount, heapSizeBytes, strategy)` creates `heapCount` Windows heaps with `HeapCreate(HEAP_NO_SERIALIZE, ...)`. Windows' own per-heap lock is disabled; AHM uses one `SRWLOCK` per heap instead.
- **Round-robin selection.** `Malloc` takes a ticket from an atomic counter (`fetch_add`, relaxed) and picks `heap = ticket % heapCount`. No extra synchronization is needed to spread the load.
- **Returning memory to the right heap.** `Free` must know which heap owns a pointer. AHM supports two interchangeable ways to do that, selected at `Initialize` time:

| Strategy | How `Free` finds the owning heap | Cost |
|---|---|---|
| `Strategy::Map` (default) | Looks up `ptr -> heapId` in `AllocationMap`, a segmented hash table (64 segments, each with its own lock, open addressing with linear probing and tombstones). | Hashing, a segment lock and probing on every `Malloc` and `Free`; extra memory for the table. |
| `Strategy::Header` | Reads a 16-byte `AllocationHeader` (`heapId`, `magic`) stored directly in front of the block. `Malloc` returns the address just after the header. | 16 bytes per allocation; no hash table and no extra locks. |

The header is 16 bytes (not 8) so that the pointer returned to the user keeps the 16-byte alignment that `HeapAlloc` provides.

Both strategies exist side by side so they can be compared directly (see [Benchmarks](#benchmarks)).

## Repository layout

```
Advanced Heap Manager/
  AHM/            static library: AdvancedHeapManager, AllocationMap, AllocationHeader, HeapEntry
  TestApp/        benchmark: allocate and free 4 GB from 1, 2, 5, 10 and 50 threads
  TestServer/     benchmark: multi-client server on loopback sockets
docs/             project specification and documentation (PDF, in Serbian)
```

## Building

Requirements: Windows, Visual Studio with the *Desktop development with C++* workload (the projects use the v143 toolset; newer Visual Studio versions may offer to retarget it).

1. Open `Advanced Heap Manager/Advanced Heap Manager.sln`.
2. Build the solution. `TestApp` and `TestServer` link against the `AHM` static library.

AHM uses the Win32 heap API, `SRWLOCK` and (in `TestServer`) Winsock, so it is Windows-only.

## Benchmarks

Both programs run three configurations back to back: standard `malloc`/`free`, AHM with `Strategy::Map` and AHM with `Strategy::Header`. Each configuration is repeated three times and the **median** is reported (the minimum and maximum are kept in the CSV output).

For meaningful numbers, run a **Release x64** build **without a debugger attached** (Visual Studio: *Debug > Start Without Debugging*, Ctrl+F5). A debugger changes how the Windows heap behaves. Close other heavy programs; each run touches several GB of memory.

### TestApp

- Allocates and then frees 4 GB in 64 KB blocks, split evenly over 1, 2, 5, 10 and 50 threads, using 8 AHM heaps.
- Every page of each block is written once so that operating-system page commit is not mixed into the measurement.
- Writes `results.csv` (columns: `strategy,threads,median_s,min_s,max_s`).
- To draw the charts:

  ```
  python -m pip install matplotlib
  python plot_results.py
  ```

  This produces `results.png` (time per thread count, and speed relative to `malloc`).

### TestServer

- A server and several clients run as threads of one process and talk over TCP on `127.0.0.1:5555`.
- Clients send messages of random size (64 B to 4 KB) and the server answers with random-size replies; both sides allocate their buffers with AHM or `malloc`.
- All clients connect first and are then released at the same moment; the timer starts at the release, so connection setup is not part of the measurement.
- The program asks for the number of clients and messages per client, prints a table (median, min, max, throughput) and appends the results to `server_results.csv`, so several client counts can be collected in one file.
- If a run prints a `UPOZORENJE` (warning) line, that run did not process the expected number of messages and should be repeated.

## Results

_Results to be added._

## Limitations

- **Windows only.**
- **`Strategy::Header` trusts the pointer it is given.** `Free` reads the 16 bytes in front of the pointer, so passing a pointer that did not come from `AHM::Malloc` is undefined behaviour. Double-free detection is best effort (the magic value is cleared on free, but two threads freeing the same pointer at the same moment can both pass the check). `Strategy::Map` is stricter: an unknown pointer is simply ignored.
- **The strategy is fixed per `Initialize` call.** A pointer must be freed under the same strategy it was allocated with. Calling `Initialize` again resets the manager.
- **`Shutdown` destroys all heaps**, which also releases any blocks that were never freed.
- `AHM::Malloc(0)` returns `nullptr`.

## Documentation

The `docs/` folder contains the original assignment and the project documentation. These documents are written in Serbian.

- `docs/Projekat24.pdf` - assignment
- `docs/AHM_Dokumentacija.pdf` - project documentation
- `docs/Dizajn_tim24.pdf` - design document
