# multithreaded-c-server

## TODO/Improvements

- Combine `ezarray` and `ezqueue` into one lib since they have common elements?
- Implement threadpool
- Dynamically reduce size of arrays to avoid memory bloat
- Do I need to pair a lock with the pollfd array? It's possible multiple threads could be acting on the same entry
  (setting POLLIN or POLLOUT).
- Figure out a strategy for max connections
- Figure out a strategy for messages greater than max buffer size
- Move b_marked_for_deletion into state struct as PENDING_DELETION
- How can this design be modified to allow jobs to task out other jobs to the threadpool?
- Need to figure out how a ctx can take a pollfd struct so that it can modify polled values
- Get rid of the double lock on check and then delete