# concurrent_sorted_list

A fairly simple lock-free sorted list supporting insert, try_pop & more. Makes use of [atomic_shared_ptr](https://github.com/FlovinTue/atomic_shared_ptr/tree/master/atomic_shared_ptr)
and also serves as an example for it's usage.

Includes needed are concurrent_sorted_list.h, atomic_oword.h, atomic_shared_ptr.h, concurrent_queue_.h, concurrent_object_pool.h
