# multithreaded-c-server

## TODO/Improvements

- Combine `ezarray` and `ezqueue` into one lib since they have common elements
- Implement threadpool
- Dynamically reduce size of arrays to avoid memory bloat
- What happens if a connection is marked for deletion, gets closed and freed, but there's still a job in the
  threadpool with a reference to (now invalid) address of conn_ctx_t? Perhaps need to implement a reference counter.
  Concerned about spinlock if refcount never hits 0 and the item keeps getting dequeued and re-queued (very very
  unlikely).
- Do I need to pair a lock with the pollfd array? It's possible multiple threads could be acting on the same entry
  (setting POLLIN or POLLOUT).
- Indices need to change in each conn_ctx_t when the conn_mgr array compacts
- **Get rid of compacting array and change add_new to fill in the first "empty" slot it finds**