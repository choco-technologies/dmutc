#ifndef TEST_SYNC_DMOD_H
#define TEST_SYNC_DMOD_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "dmutc_defs.h"
typedef struct { int unused; } Dmod_Context_t;
extern const char* dmod_dmutc_read_source_sig;
Dmod_Context_t* Dmod_GetNextDifModule(const char*, Dmod_Context_t*);
void (*Dmod_GetDifFunction(Dmod_Context_t*, const char*))(void);
#define DMOD_LOG_INFO(...) ((void)0)
#endif
