#pragma once

#include <inttypes.h>

namespace rpc {
namespace common {

inline void barrier() {
    asm volatile("" : : : "memory");
}

inline void relax_fence() {
    asm volatile("pause" : : : "memory"); // equivalent to "rep; nop"
}

inline uint8_t xchg(uint8_t *object, uint8_t new_value) {
    asm volatile("xchgb %0,%1"
		     : "+q" (new_value), "+m" (*object));
    barrier();
    return new_value;
}

struct spinlock {
    uint8_t locked;   // Is the lock held?
};

inline void initlock(struct spinlock *lock) {
    lock->locked = 0;
}

// Acquire the lock.
// Loops (spins) until the lock is acquired.
// Holding a lock for a long time may cause
// other CPUs to waste time spinning to acquire it.
inline void acquire(struct spinlock *lock) {
    // The xchg is atomic.
    // It also serializes, so that reads after acquire are not
    // reordered before it.
    while(xchg(&lock->locked, (uint8_t)1) == 1)
      relax_fence();
}

// Release the lock.
inline void release(struct spinlock *lock) {
    // The xchg serializes, so that reads before release are
    // not reordered after it.  (This reordering would be allowed
    // by the Intel manuals, but does not happen on current
    // Intel processors.  The xchg being asm volatile also keeps
    // gcc from delaying the above assignments.)
    xchg(&lock->locked, (uint8_t)0);
}

} // namespace common
} // namespace rpc

