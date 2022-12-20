#include "mpthreadport.h"
#include "py/mpstate.h"
#include "py/runtime.h"
#include <3ds.h>
#include <stdlib.h>

static _Thread_local mp_state_thread_t* thread_current_state = NULL;
mp_state_thread_t *mp_thread_get_state(void) {
    return thread_current_state;
}
void mp_thread_set_state(mp_state_thread_t *state) {
    thread_current_state = state;
}

void mp_thread_mutex_init(mp_thread_mutex_t *mutex) {
    LightLock_Init(mutex);
}

int mp_thread_mutex_lock(mp_thread_mutex_t *mutex, int wait) {
    return LightLock_TryLock(mutex) == 0;
}

void mp_thread_mutex_unlock(mp_thread_mutex_t *mutex) {
    LightLock_Unlock(mutex);
}

// this structure forms a linked list, one node per active thread
typedef struct _mp_thread_t {
    Thread handle;          // system id of thread
    void *(*entry)(void *);
    void *arg;              // thread Python args, a GC root pointer
    struct _mp_thread_t *next;
} mp_thread_t;

static mp_thread_mutex_t thread_mutex;
static mp_thread_t *thread_lifo;

static void my_thread_func(void* th_arg)
{
    mp_thread_t* th = (mp_thread_t*)th_arg;
    th->entry(th->arg);
}

void mp_thread_create(void *(*entry)(void *), void *arg, size_t *stack_size)
{
    if (*stack_size == 0) {
        *stack_size = (0x3000 * sizeof(void *)) - 1024;
    }

    mp_thread_t* th = (mp_thread_t*)malloc(sizeof(mp_thread_t));
    th->entry = entry;
    th->arg = arg;

    mp_thread_mutex_lock(&thread_mutex, 0);
    th->handle = threadCreate(&my_thread_func, th, *stack_size + 1024, 0x30, 0, 0);
    if(th->handle == NULL)
    {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("unable to start a new thread"));
    }
    else
    {
        thread_lifo = th;
        th->next = thread_lifo;
    }
    mp_thread_mutex_unlock(&thread_mutex);
}

void mp_thread_start(void)
{
    // empty
}
void mp_thread_finish(void)
{
    // empty
}

void mp_thread_init(void) {
    thread_lifo = NULL;
    mp_thread_mutex_init(&thread_mutex);
    mp_thread_set_state(&mp_state_ctx.thread);
}

void mp_thread_deinit(void) {
    mp_thread_mutex_lock(&thread_mutex, 0);
    for(mp_thread_t *th = thread_lifo; th != NULL;) {
        threadJoin(th->handle, -1);
        threadFree(th->handle);
        mp_thread_t *next = th->next;
        free(th);
        th = next;
    }
    thread_lifo = NULL;
    mp_thread_mutex_unlock(&thread_mutex);
}