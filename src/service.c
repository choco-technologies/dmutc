#include "dmod.h"
#include "dmosi.h"
#include "service.h"
#include <errno.h>
#include <string.h>

struct source {
    struct source* next;
    char* unit;
    char* path;
};
static struct source* sources;
static dmosi_mutex_t lock;
static char* copy(const char* value)
{
    size_t length = strlen(value) + 1;
    char* out = Dmod_Malloc(length);
    if (out != NULL) memcpy(out, value, length);
    return out;
}
static void free_source(struct source* source)
{
    Dmod_Free(source->unit); Dmod_Free(source->path); Dmod_Free(source);
}
int dmutc_sources_init(void)
{
    if (lock != NULL) return 0;
    lock = dmosi_mutex_create(false);
    return lock == NULL ? -ENOMEM : 0;
}
int dmutc_sources_deinit(void)
{
    if (lock == NULL) return 0;
    int rc = dmosi_mutex_lock(lock);
    if (rc != 0) return rc;
    while (sources != NULL) {
        struct source* old = sources; sources = old->next; free_source(old);
    }
    dmosi_mutex_unlock(lock); dmosi_mutex_destroy(lock); lock = NULL;
    return 0;
}
int dmutc_start_source(const char* unit, int argc, const char* const argv[])
{
    if (unit == NULL || *unit == '\0' || argc != 2 || argv == NULL ||
        argv[0] == NULL || argv[1] == NULL || argv[1][0] != '/') return -EINVAL;
    if (lock == NULL) return -ENODEV;
    struct source* candidate = Dmod_Malloc(sizeof(*candidate));
    if (candidate == NULL) return -ENOMEM;
    candidate->unit = copy(unit); candidate->path = copy(argv[1]); candidate->next = NULL;
    if (candidate->unit == NULL || candidate->path == NULL) {
        free_source(candidate); return -ENOMEM;
    }
    int rc = dmosi_mutex_lock(lock);
    if (rc != 0) { free_source(candidate); return rc; }
    rc = dmutc_probe_source(candidate->path); /* An unset RTC is still a valid source. */
    if (rc == 0) {
        struct source** slot = &sources;
        while (*slot != NULL && strcmp((*slot)->unit, unit) < 0) slot = &(*slot)->next;
        if (*slot != NULL && strcmp((*slot)->unit, unit) == 0) {
            struct source* old = *slot;
            candidate->next = old->next; *slot = candidate; free_source(old);
        } else { candidate->next = *slot; *slot = candidate; }
    }
    dmosi_mutex_unlock(lock);
    if (rc != 0) free_source(candidate);
    return rc;
}
int dmutc_stop_source(const char* unit)
{
    if (unit == NULL) return -EINVAL;
    if (lock == NULL) return -ENODEV;
    int rc = dmosi_mutex_lock(lock);
    if (rc != 0) return rc;
    struct source** slot = &sources;
    while (*slot != NULL && strcmp((*slot)->unit, unit) != 0) slot = &(*slot)->next;
    if (*slot == NULL) rc = -ENOENT;
    else { struct source* old = *slot; *slot = old->next; free_source(old); }
    dmosi_mutex_unlock(lock);
    return rc;
}
int dmutc_lock_source(const char** path)
{
    if (lock == NULL) return -ENODEV;
    int rc = dmosi_mutex_lock(lock);
    if (rc != 0) return rc;
    if (sources == NULL) { dmosi_mutex_unlock(lock); return -ENODEV; }
    *path = sources->path;
    return 0;
}
void dmutc_unlock_source(void) { dmosi_mutex_unlock(lock); }
