#pragma once

#include <string_view>
#include <optional>
#include <string>
#include <atomic>
#include <queue>
#include <span>

#include <3ds.h>
#include "ctr_thread.h"

struct python_handler {
    python_handler(std::span<std::string_view> import_search_paths);
    ~python_handler();

    template<typename T>
    void write(const T& val)
    {
        std::unique_lock lk(in_queue_mut);
        in_text.emplace(val);
        line_done = false;
        LightEvent_Signal(&new_event);
    }

    /*
     * 1 = read a string
     * 0 = nothing to read, finished
     * -1 = nothing to read, calculating
     */
    int read(std::string& into);

    // exit code when SystemExit raised
    std::optional<int> should_exit() const;
    void signal_stop();
    void signal_interrupt();

private:
    ctr::thread self_thread;
    std::queue<std::string> in_text;
    std::queue<std::string> out_text;
    ctr::mutex in_queue_mut;
    ctr::mutex out_queue_mut;
    LightEvent stop_event, new_event;
    std::atomic_bool line_done;
    std::optional<int> should_exit_opt;
    std::span<std::string_view> import_search_paths;

    void handle_print(std::string_view str)
    {
        std::unique_lock lk(out_queue_mut);
        out_text.emplace(str);
    }

    static void print_callback(void* handler, std::string_view str)
    {
        python_handler* py_handler = (python_handler*)handler;
        py_handler->handle_print(str);
    }

    void loop_func();
};
