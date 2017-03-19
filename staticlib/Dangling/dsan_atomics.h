#ifndef DSAN_ATOMICS_H
#define DSAN_ATOMICS_H

#include <stdatomic.h>

/* Consistently add value into the addr.
 * TODO: Using sequential consistent memory model(strict). We 
 * will go for the relaxed one if required.
 */ 
static inline __attribute__((always_inline))
unsigned long dang_atomic_add(unsigned long *addr, int val)
{
    unsigned long expected, desired;
    //expected = __atomic_load_8(addr, __ATOMIC_SEQ_CST);
    expected = atomic_load((_Atomic(unsigned long)*)addr);
    do {
        desired = expected + val;
    //} while (!__atomic_compare_exchange_8(addr, &expected, desired, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));
    } while (!atomic_compare_exchange_strong((_Atomic(unsigned long)*)addr, &expected, desired));
    return desired;
}

/* 
 * Consistently, write desired value into the addr.
 * Return old value of addr. Using strict sequential consistency.
 */
static inline __attribute__((always_inline))
unsigned long dang_atomic_cmpxchng(unsigned long *addr, unsigned long desired)
{
    unsigned long expected;
    do {
        //expected = __atomic_load_8(addr, __ATOMIC_SEQ_CST);
        expected = atomic_load((_Atomic(unsigned long)*)addr);
    //} while (!__atomic_compare_exchange_8(addr, &expected, desired, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));
    } while (!atomic_compare_exchange_strong((_Atomic(unsigned long)*)addr, &expected, desired));

    return expected;
}

static inline __attribute__((always_inline))
void dang_atomic_cmpxchng_once(unsigned long *addr, unsigned long expected, unsigned long desired)
{
    atomic_compare_exchange_strong((_Atomic(unsigned long)*)addr, &expected, desired);
    return;
}
        
#endif /* !DSAN_ATOMICS_H */
