#ifndef TEST_SYNC_DMOSI_H
#define TEST_SYNC_DMOSI_H
#include <stddef.h>
#include <stdint.h>
typedef void* dmosi_thread_t;
typedef void* dmosi_semaphore_t;
/* No scheduler in this synchronous API test. */
static inline dmosi_semaphore_t dmosi_semaphore_create(unsigned a, unsigned b) {(void)a;(void)b;return NULL;}
static inline int dmosi_semaphore_wait(void* s, unsigned n, int32_t ms) {(void)s;(void)n;(void)ms;return 0;}
static inline int dmosi_semaphore_post(void* s, unsigned n) {(void)s;(void)n;return 0;}
static inline void dmosi_semaphore_destroy(void* s) {(void)s;}
static inline dmosi_thread_t dmosi_thread_create(void (*f)(void*), void* a, int p, unsigned n, const char* name, void* u) {(void)f;(void)a;(void)p;(void)n;(void)name;(void)u;return NULL;}
static inline int dmosi_thread_join(void* t) {(void)t;return 0;}
static inline void dmosi_thread_destroy(void* t) {(void)t;}
#endif
