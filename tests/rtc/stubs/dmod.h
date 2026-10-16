#ifndef TEST_DMOD_H
#define TEST_DMOD_H
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "dmutc_defs.h"
typedef struct Dmod_Config Dmod_Config_t;
void* Dmod_FileOpen(const char* path, const char* mode);
void Dmod_FileClose(void* file);
int Dmod_Ioctl(void* file, int command, void* arg);
#endif
