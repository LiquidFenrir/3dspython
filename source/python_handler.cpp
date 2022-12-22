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
template<typename T>
static int do_run(T&& callback)
{
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        callback();
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
            Printer::print("\e[31m");
            mp_obj_print_exception(&mp_plat_print, MP_OBJ_FROM_PTR(nlr.ret_val));
            Printer::print("\e[0m");
        }
        return -1;
    }
}

python_handler::python_handler(std::span<std::string_view> import_search_paths_arg)
    : import_search_paths(import_search_paths_arg)
{
    LightEvent_Init(&stop_event, RESET_ONESHOT);
    LightEvent_Init(&new_event, RESET_ONESHOT);
    line_done = false;

    Printer::payload = this;
    Printer::callback = &python_handler::print_callback;

    ctr::thread::meta meta = ctr::thread::basic_meta;
    meta.stack_size = 80 * 1024;
    meta.prio += 1;
    self_thread = ctr::thread(meta, &python_handler::loop_func, this);
}

python_handler::~python_handler()
{
    signal_stop();
    self_thread.join();
}

int python_handler::read(std::string& into)
{
    std::unique_lock lk(out_queue_mut);
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
    // stops the loop
    LightEvent_Signal(&stop_event);
    // starts a new iteration to notice the loop should stop
    LightEvent_Signal(&new_event);
    // stops running code if any, to cause a new iteration
    signal_interrupt();
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

    line_done = true;
    auto repl_locals = mp_locals_get();
    auto repl_globals = mp_globals_get();

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

        if(!line.empty())
        {
            const auto run_file_callback = [&]() {
                mp_lexer_t *lex = mp_lexer_new_from_file(line.c_str() + 1);
                mp_obj_t file_globals = mp_obj_new_dict(1);
                mp_obj_t file_locals = mp_obj_new_dict(1);
                mp_parse_compile_execute(lex, MP_PARSE_FILE_INPUT, (mp_obj_dict_t*)MP_OBJ_TO_PTR(file_globals), (mp_obj_dict_t*)MP_OBJ_TO_PTR(file_locals));
            };
            const auto run_line_callback = [&]() {
                mp_lexer_t *lex = mp_lexer_new_from_str_len(MP_QSTR__lt_stdin_gt_, line.c_str(), line.size(), 0);
                mp_parse_compile_execute(lex, MP_PARSE_SINGLE_INPUT, repl_globals, repl_locals);
            };
            const int r = line.front() == '\0' ? do_run(run_file_callback) : do_run(run_line_callback);
            if(r > 0 && r & FORCED_EXIT)
            {
                should_exit_opt = r & 0xff;
            }
        }
        line_done = true;
    }

    mp_thread_deinit();
    mp_deinit();
}
