#include "python_handler.h"
#include "printer.h"

extern "C" {
#include "py/builtin.h"
#include "py/compile.h"
#include "py/runtime.h"
#include "py/obj.h"
#include "py/objstr.h"
#include "py/gc.h"
#include "py/stackctrl.h"
#include "py/mperrno.h"
#include "py/nlr.h"
#include "extmod/vfs.h"
#include "extmod/vfs_posix.h"
}

#define FORCED_EXIT (0x100)
static int do_str(std::string_view src, mp_parse_input_kind_t input_kind = MP_PARSE_SINGLE_INPUT)
{
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        mp_lexer_t *lex = mp_lexer_new_from_str_len(MP_QSTR__lt_stdin_gt_, src.data(), src.size(), 0);
        qstr source_name = lex->source_name;
        mp_parse_tree_t parse_tree = mp_parse(lex, input_kind);
        mp_obj_t module_fun = mp_compile(&parse_tree, source_name, input_kind == MP_PARSE_SINGLE_INPUT);
        mp_call_function_0(module_fun);
        nlr_pop();
        return 0;
    } else {
        auto exc = (mp_obj_base_t *)nlr.ret_val;
        // uncaught exception
        if (mp_obj_is_subclass_fast(MP_OBJ_FROM_PTR(exc->type), MP_OBJ_FROM_PTR(&mp_type_SystemExit))) {
            // None is an exit value of 0; an int is its value; anything else is 1
            mp_obj_t exit_val = mp_obj_exception_get_value(MP_OBJ_FROM_PTR(exc));
            mp_int_t val = 0;
            if (exit_val != mp_const_none && !mp_obj_get_int_maybe(exit_val, &val)) {
                val = 1;
            }
            return FORCED_EXIT | (val & 255);
        }
        else
        {
            mp_obj_print_exception(&mp_plat_print, (mp_obj_t)nlr.ret_val);
        }
        return -1;
    }
}

python_handler::python_handler(std::span<std::string_view> import_search_paths_arg)
    : import_search_paths(import_search_paths_arg)
{
    LightEvent_Init(&stop_event, RESET_ONESHOT);
    LightEvent_Init(&new_event, RESET_ONESHOT);
    line_done = true;

    ctr::thread::meta meta = ctr::thread::basic_meta;
    meta.stack_size = 80 * 1024;
    meta.prio += 1;
    self_thread = ctr::thread(meta, &python_handler::loop_func, this);
    Printer::payload = this;
    Printer::callback = &python_handler::print_callback;
}

python_handler::~python_handler()
{
    signal_stop();
    self_thread.join();
}

int python_handler::read(std::string& into)
{
    if(out_text.empty())
    {
        return line_done ? 0 : -1;
    }
    else
    {
        into = std::move(out_text.front());
        out_text.pop();
        return 1;
    }
}

std::optional<int> python_handler::should_exit() const
{
    return should_exit_opt;
}

void python_handler::signal_stop()
{
    LightEvent_Signal(&stop_event);
    LightEvent_Signal(&new_event);
}
void python_handler::signal_interrupt()
{
    mp_sched_keyboard_interrupt();
}

void python_handler::handle_print(std::string_view str)
{
    std::unique_lock lk(out_queue_mut);
    out_text.emplace(str);
}

void python_handler::print_callback(void* handler, std::string_view str)
{
    python_handler* py_handler = (python_handler*)handler;
    py_handler->handle_print(str);
}

void python_handler::loop_func()
{
    const std::size_t heap_size = 1 << 20, stack_size = 40960;
    auto heap_holder = std::make_unique<char[]>(heap_size);

    mp_thread_init();
    mp_stack_ctrl_init();
    mp_stack_set_limit(stack_size - 1024);
    gc_init(heap_holder.get(), heap_holder.get() + heap_size);

    mp_init();

    // Mount the host FS at the root of our internal VFS
    mp_obj_t args[2] = {
        MP_OBJ_TYPE_GET_SLOT(&mp_type_vfs_posix, make_new)(&mp_type_vfs_posix, 0, 0, NULL),
        MP_OBJ_NEW_QSTR(MP_QSTR__slash_),
    };
    mp_vfs_mount(2, args, (mp_map_t *)&mp_const_empty_map);
    MP_STATE_VM(vfs_cur) = MP_STATE_VM(vfs_mount_table);

    mp_obj_list_init((mp_obj_list_t*)MP_OBJ_TO_PTR(mp_sys_path), 0);
    for(const auto s : import_search_paths)
    {
        mp_obj_list_append(mp_sys_path, mp_obj_new_str(s.data(), s.size()));
    }
    mp_obj_list_init((mp_obj_list_t*)MP_OBJ_TO_PTR(mp_sys_argv), 0);

    std::string line;
    while(true)
    {
        LightEvent_Wait(&new_event);
        if(LightEvent_TryWait(&stop_event))
        {
            break;
        }

        {
        std::unique_lock lk(in_queue_mut);
        line = std::move(in_text.front());
        in_text.pop();
        }

        const int r = do_str(line);
        if(r > 0 && r & FORCED_EXIT)
        {
            should_exit_opt = r & 0xff;
        }
        line_done = true;
    }

    mp_thread_deinit();
    mp_deinit();
}
