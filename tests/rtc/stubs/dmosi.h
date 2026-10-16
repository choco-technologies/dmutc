#ifndef TEST_DMOSI_H
#define TEST_DMOSI_H
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
typedef pthread_mutex_t* dmosi_mutex_t;
static inline dmosi_mutex_t dmosi_mutex_create(bool recursive) {
    (void)recursive; dmosi_mutex_t m=malloc(sizeof(*m));
    if(m!=NULL && pthread_mutex_init(m,NULL)!=0) {free(m);m=NULL;} return m;
}
static inline int dmosi_mutex_lock(dmosi_mutex_t m) {return -pthread_mutex_lock(m);}
static inline int dmosi_mutex_unlock(dmosi_mutex_t m) {return -pthread_mutex_unlock(m);}
static inline void dmosi_mutex_destroy(dmosi_mutex_t m) {pthread_mutex_destroy(m);free(m);}
#endif
