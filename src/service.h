#ifndef DMUTC_SERVICE_H
#define DMUTC_SERVICE_H
int dmutc_sources_init(void);
int dmutc_sources_deinit(void);
int dmutc_start_source(const char* unit, int argc, const char* const argv[]);
int dmutc_stop_source(const char* unit);
int dmutc_lock_source(const char** path);
void dmutc_unlock_source(void);
int dmutc_probe_source(const char* path);
int dmutc_sync_start(void);
void dmutc_sync_stop(void);
void dmutc_sync_now(void);
#endif
