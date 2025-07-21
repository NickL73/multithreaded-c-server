# multithreaded-c-server

## TODO/Improvements

- Combine `ezarray` and `ezqueue` into one lib since they have common elements?
- Implement threadpool
- Dynamically reduce size of arrays to avoid memory bloat
- Do I need to pair a lock with the pollfd array? It's possible multiple threads could be acting on the same entry
  (setting POLLIN or POLLOUT).
- Figure out a strategy for max connections