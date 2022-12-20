#include "py/mphal.h"
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

void mp_hal_delay_us(mp_uint_t us) {
    usleep(us);
}

mp_uint_t mp_hal_ticks_ms(void) {
#if (defined(_POSIX_TIMERS) && _POSIX_TIMERS > 0) && defined(_POSIX_MONOTONIC_CLOCK)
    struct timespec tv;
    clock_gettime(CLOCK_MONOTONIC, &tv);
    return tv.tv_sec * 1000 + tv.tv_nsec / 1000000;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
}
