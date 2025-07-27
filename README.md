# multithreaded-c-server

## TODO/Improvements

- Combine `ezarray` and `ezqueue` into one lib since they have common elements?
- Dynamically reduce size of arrays to avoid memory bloat
- Get rid of the sig_atomic_t and just rely on the self pipe for graceful shutdown
- Figure out a strategy for max connections
- Figure out a strategy for messages greater than max buffer size
- How can this design be modified to allow jobs to task out other jobs to the threadpool?
