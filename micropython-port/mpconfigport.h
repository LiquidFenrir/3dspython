#include <stdint.h>

// options to control how MicroPython is built

#define MICROPY_CONFIG_ROM_LEVEL                (MICROPY_CONFIG_ROM_LEVEL_FULL_FEATURES)

// You can disable the built-in MicroPython compiler by setting the following
// config option to 0.  If you do this then you won't get a REPL prompt, but you
// will still be able to execute pre-compiled scripts, compiled with mpy-cross.
#define MICROPY_ENABLE_COMPILER                 (1)

// #define MICROPY_QSTR_EXTRA_POOL                 mp_qstr_frozen_const_pool
#define MICROPY_ENABLE_GC                       (1)
#define MICROPY_HELPER_REPL                     (1)
#define MICROPY_MODULE_FROZEN_MPY               (0)
#define MICROPY_MODULE_FROZEN_STR               (0)
#define MICROPY_ENABLE_EXTERNAL_IMPORT          (1)

#define MICROPY_ALLOC_PATH_MAX                  (256)
#define MICROPY_ALLOC_PARSE_CHUNK_INIT          (16)
#define MICROPY_REPL_EVENT_DRIVEN               (1)
#define MICROPY_PY_ALL_INPLACE_SPECIAL_METHODS  (1)
#define MICROPY_NLR_SETJMP                      (1)

#define MICROPY_FLOAT_IMPL                      (MICROPY_FLOAT_IMPL_FLOAT)
#define MICROPY_LONGINT_IMPL                    (MICROPY_LONGINT_IMPL_MPZ)
#define MP_SSIZE_MAX                            ((SIZE_MAX) >> 1)
#define MICROPY_PY_BUILTINS_HELP                (0)
#define MICROPY_PY_THREAD                       (1)
#define MICROPY_PY_SYS_STDFILES                 (0)
#define MICROPY_PY_SYS_PATH_ARGV_DEFAULTS       (0)
#define MICROPY_PY_SYS_PS1_PS2                  (0)

#define MICROPY_VFS                             (1)
#define MICROPY_VFS_POSIX                       (1)
#define MICROPY_READER_VFS                      (1)
#define MICROPY_PY_BUILTINS_INPUT               (0)
#define MICROPY_PY_ALL_INPLACE_SPECIAL_METHODS  (1)
#define MICROPY_PY_URE_MATCH_GROUPS             (1)
// #define MICROPY_DEBUG_VERBOSE                   (1)

#define MICROPY_PORT_BUILTINS \
    { MP_ROM_QSTR(MP_QSTR_input), MP_ROM_PTR(&mp_builtin_input_obj) },

// type definitions for the specific machine
typedef intptr_t mp_int_t; // must be pointer size
typedef uintptr_t mp_uint_t; // must be pointer size
typedef long mp_off_t;

// We need to provide a declaration/definition of alloca()
#include <alloca.h>

#define MICROPY_HW_BOARD_NAME "ninty3ds"
#define MICROPY_HW_MCU_NAME "mpcore"

#define MP_STATE_PORT MP_STATE_VM

#include <errno.h>

#define MICROPY_EVENT_POLL_HOOK \
    do { \
        extern void mp_handle_pending(bool); \
        extern void mp_hal_delay_us(mp_uint_t us); \
        mp_handle_pending(true); \
        mp_hal_delay_us(250); \
    } while (0);

// This macro is used to implement PEP 475 to retry specified syscalls on EINTR
#define MP_HAL_RETRY_SYSCALL(ret, syscall, raise) \
    { \
        for (;;) { \
            extern void mp_handle_pending(bool); \
            ret = syscall; \
            if (ret == -1) { \
                int err = errno; \
                if (err == EINTR) { \
                    mp_handle_pending(true); \
                    continue; \
                } \
                raise; \
            } \
            break; \
        } \
    }
