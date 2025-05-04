# A proposed design for thread-safe worlds (levels)

@bridgekat

For the scope of this project, it is expected that the world will remain as a collection *chunks* each consisting of 16x16x16 or 32x32x32 blocks, plus a few caching structures for faster terrain generation and block lookup.

## Two-phase locking scheme

In parallel block updates, multiple threads access the world, some of which may be writers. To ensure every thread sees a consistent local state, we can enforce two-phase locking on the chunk container and individual chunks. For every thread $T$ which access a subset of chunks:

1. $T$ acquires the chunk container lock (which also covers the caching structures).
2. $T$ acquires locks on all chunks it wishes to access (including creation and removal), in any order.
3. $T$ releases the chunk container lock.
4. $T$ performs operations on the locked chunks. This stage is expected to take the longest time.
5. $T$ can release chunk locks in any order.

To avoid contention, all locks can be RW-locks. Still, a writer thread waiting on the chunks it wishes to access can block all subsequent accesses to the chunk container. One solution is to queue writing tasks (e.g. block updates, chunk unloads) and attempt them repeatedly instead of blocking on the acquisition of locks. Read operations are allowed to cancel themselves (e.g. simply return a placeholder value) instead of blocking on the acquisition of locks.

## Improved locking scheme

If the above design did not yield satisafactory performance, we may implement a customized hash map using atomic reference counting on the arrays. Each slot in the array contains a raw chunk pointer together with the entry lock (which now guards the chunk *as well as* its pointer). Now for every thread $T$ which access a subset of chunks:

1. $T$ duplicates the `atomic<shared_ptr>`[^1] to the current array.
2. $T$ acquires locks on all entries it wishes to access, in increasing order of index.
  - After acquiring the first lock, $T$ checks if the current array is still the one it is holding. If not, start over.
3. $T$ performs operations on the locked entries.
4. $T$ can release entry locks in any order.
5. $T$ drops the `shared_ptr`.

For every thread $T$ that wants to extend or shrink the array:

1. $T$ duplicates the `atomic<shared_ptr>` to the current array.
2. $T$ allocates a new array via `shared_ptr`.
3. $T$ acquires all **write (exclusive)** locks on the current array, in increasing order of index.
  - After acquiring the first lock, $T$ checks if the current array is still the one it is holding. If not, start over.
4. $T$ rehashes all entries from the current array to the new array.
5. $T$ sets the current `atomic<shared_ptr>` to point to the new array, dropping its ownership to the new array.
6. $T$ drops all locks, and then its ownership to the old array.

Note the 2PL rule still applies to chunks and chunk pointers, and every access is guarded by a reference count (so no use-after-free). This allows us to omit locking the container, since all its entries are guarded by immutable locks, and every thread is effectively read-only on the arrays once it is initialised and pointed by the `atomic<shared_ptr>`. The use of `atomic<shared_ptr>` allows us to safely orphan an array. Therefore, the possibility of data race is excluded.

However, we would also assume that the current array owns all chunk pointers. To avoid breaking ownership, array-orphaning threads must acquire all **write (exclusive)** locks on the old array for a consistent copy. This should not only block all other threads, but also prevent them to ever access the contents in the old array (which can include invalidated chunk pointers) by forcing them to retry, even after the locks are released.

[^1]: See https://en.cppreference.com/w/cpp/memory/shared_ptr and https://en.cppreference.com/w/cpp/memory/shared_ptr/atomic2.
