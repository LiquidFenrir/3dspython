#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <3ds.h>

#include "py/builtin.h"
#include "py/compile.h"
#include "py/runtime.h"
#include "py/repl.h"
#include "py/gc.h"
#include "py/stackctrl.h"
#include "py/mperrno.h"
#include "shared/readline/readline.h"

static int my_readline(vstr_t *line, const char *prompt) {
    SwkbdState swkbd;
    char* buf = (char*)calloc(1,1024);
    swkbdInit(&swkbd, SWKBD_TYPE_WESTERN, 2, 384);
    swkbdSetHintText(&swkbd, prompt);
    swkbdSetButton(&swkbd, SWKBD_BUTTON_LEFT, "Int.", true);
    // swkbdSetButton(&swkbd, SWKBD_BUTTON_MIDDLE, "EOF", true);
    swkbdSetButton(&swkbd, SWKBD_BUTTON_RIGHT, "Enter", true);
    SwkbdButton res = swkbdInputText(&swkbd, buf, 1024);
    if(res == SWKBD_BUTTON_LEFT)
    {
        vstr_init(line, 0);
        free(buf);
        return CHAR_CTRL_C;
    }
    else if(res == SWKBD_BUTTON_MIDDLE)
    {
        vstr_init(line, 0);
        free(buf);
        return CHAR_CTRL_D;
    }
    else
    {
        vstr_init(line, 0);
        vstr_add_str(line, buf);
        free(buf);
        return 0;
    }
}

static mp_obj_t my_input_builtin(size_t n_args, const mp_obj_t *args, mp_map_t *kwargs) {
    const char* prompt = "";
    if (n_args == 1) {
        prompt = mp_obj_str_get_str(args[0]);
    }

    vstr_t line;
    int ret = my_readline(&line, prompt);
    if (ret == CHAR_CTRL_C) {
        mp_raise_type(&mp_type_KeyboardInterrupt);
    }
    if (line.len == 0 && ret == CHAR_CTRL_D) {
        mp_raise_type(&mp_type_EOFError);
    }
    return mp_obj_new_str_from_vstr(&line);
}
MP_DEFINE_CONST_FUN_OBJ_KW(mp_builtin_input_obj, 1, my_input_builtin);

void mp_hal_stdout_tx_strn(const char *str, mp_uint_t len) {
    extern void app_recv_str(const char *, size_t);
    app_recv_str(str, len);
}

void gc_collect(void) {
    gc_collect_start();
    const mp_uint_t stack_use = mp_stack_usage();
    void** sp = (void**)((uintptr_t)MP_STATE_THREAD(stack_top) - stack_use);
    gc_collect_root(sp, stack_use);
    gc_collect_end();
}

void nlr_jump_fail(void *val) {
    while (1) {
        ;
    }
}

void NORETURN __fatal_error(const char *msg) {
    fprintf(stderr, "FATAL: %s\n", msg);
    while (1) {
        ;
    }
}

#ifndef NDEBUG
void MP_WEAK __assert_func(const char *file, int line, const char *func, const char *expr) {
    fprintf(stderr, "Assertion '%s' failed, at file %s:%d\n", expr, file, line);
    __fatal_error("Assertion failed");
}
#endif
