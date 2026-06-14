README

asgn3  implements a thread-safe queue and reader-writer lock in C using pthreads.


queue.c
Implements a bounded queue using a circular buffer.

Features:
- Thread-safe queue using `pthread_mutex_t`
- Uses condition variables to block when:
  - queue is full (`not_full`)
  - queue is empty (`not_empty`)
- Supports:
  - `queue_new()` to create a queue
  - `queue_delete()` to free the queue
  - `queue_push()` to add elements
  - `queue_pop()` to remove elements
- Uses modulus (`%`) for circular buffer indexing

rwlock.c
Implements a reader-writer lock using mutexes and condition variables.

Features:
- Multiple readers can access at the same time
- Writers get exclusive access
- Uses:
  - `reader_lock()` / `reader_unlock()`
  - `writer_lock()` / `writer_unlock()`
- Tracks active and waiting readers/writers
- Uses condition variables to synchronize threads

Both files use `pthread_mutex_t` and `pthread_cond_t` for synchronization.