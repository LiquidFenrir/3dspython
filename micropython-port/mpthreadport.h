#include <stdint.h>
#include <3ds/synchronization.h>

typedef LightLock mp_thread_mutex_t;

void mp_thread_init(void);
void mp_thread_gc_others(void);
void mp_thread_deinit(void);
