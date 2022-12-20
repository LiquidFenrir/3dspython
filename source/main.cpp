#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <cmath>

#include <string_view>
#include <string>
#include <memory>
#include <vector>
#include <span>
#include <queue>
#include <array>
#include <optional>
#include <algorithm>

#include <3ds.h>
#include <citro3d.h>
#include <citro2d.h>
#include "ctr_thread.h"

extern "C" {
#include "py/builtin.h"
#include "py/compile.h"
#include "py/runtime.h"
#include "py/repl.h"
#include "py/obj.h"
#include "py/objstr.h"
#include "py/gc.h"
#include "py/stackctrl.h"
#include "py/mperrno.h"
#include "py/nlr.h"
#include "extmod/vfs.h"
#include "extmod/vfs_posix.h"
}

namespace Printer {
using payload_t = void*;
using callback_t = void (*)(payload_t, std::string_view);
static payload_t payload = nullptr;
static callback_t callback = nullptr;
}
extern "C" void app_recv_str(const char* str, size_t len)
{
    if(Printer::callback)
    {
        Printer::callback(Printer::payload, {str, len});
    }
}

#define FORCED_EXIT (0x100)
int do_str(std::string_view src, mp_parse_input_kind_t input_kind = MP_PARSE_SINGLE_INPUT)
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

extern "C" int DEBUG_printf(const char* format, ...)
{
    va_list vl;
    va_start(vl, format);
    const int ret = vfprintf(stderr, format, vl);
    va_end(vl);
    return ret;
}

struct python_handler {
    python_handler()
    {
        LightEvent_Init(&stop_event, RESET_ONESHOT);
        LightEvent_Init(&new_event, RESET_ONESHOT);
        line_done = true;
    }

    template<typename T>
    void write(T&& val)
    {
        std::unique_lock lk(in_queue_mut);
        in_text.emplace(std::forward<T>(val));
        line_done = false;
        LightEvent_Signal(&new_event);
    }

    std::optional<std::string> read()
    {
        std::unique_lock lk(out_queue_mut);
        if(out_text.empty())
        {
            return std::nullopt;
        }
        else
        {
            auto out = std::make_optional(out_text.front());
            out_text.pop();
            return out;
        }
    }

    bool is_line_done() const
    {
        return line_done;
    }
    std::optional<int> should_exit() const
    {
        return should_exit_opt;
    }

    void signal_stop()
    {
        LightEvent_Signal(&stop_event);
        LightEvent_Signal(&new_event);
    }
    void signal_interrupt()
    {
        mp_sched_keyboard_interrupt();
    }

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

    void loop_func()
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

        const std::string_view paths_arr[] = {
            "sdmc:/python-lib",
            "sdmc:/python-work",
        };

        mp_obj_list_init((mp_obj_list_t*)MP_OBJ_TO_PTR(mp_sys_path), 0);
        for(const auto s : paths_arr)
        {
            mp_obj_list_append(mp_sys_path, mp_obj_new_str(s.data(), s.size()));
        }
        mp_obj_list_init((mp_obj_list_t*)MP_OBJ_TO_PTR(mp_sys_argv), 0);

        while(true)
        {
            LightEvent_Wait(&new_event);
            if(LightEvent_TryWait(&stop_event))
            {
                break;
            }

            std::string line;
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
private:
    std::queue<std::string> in_text;
    std::queue<std::string> out_text;
    ctr::mutex in_queue_mut;
    ctr::mutex out_queue_mut;
    LightEvent stop_event, new_event;
    std::atomic_bool line_done;
    std::optional<int> should_exit_opt;
};
struct keyboard {
    static constexpr inline int BUTTON_H = 30;
    static constexpr inline int DRAWN_BUTTON_H = 26;
    struct key {
        int x, y, w;
        int symbol;
    };
    using pane = std::vector<key>;
    std::vector<pane> panes;
    unsigned shift_state;

    int do_press(int x, int y, std::string& into, unsigned& cursor)
    {
        auto do_action = [this, &into, &cursor](char c)
        {
            if(c < 0x20)
            {
                switch(c)
                {
                case '\x01':
                    switch(shift_state & 0x5)
                    {
                    case 0:
                        shift_state ^= 1;
                        break;
                    case 1:
                        shift_state ^= 4;
                        break;
                    case 5:
                        shift_state ^= 5;
                        break;
                    default:
                        break;
                    }
                    return 0;
                case '\x02':
                    shift_state &= ~5;
                    shift_state ^= 2;
                    return 0;
                case '\x03':
                    if(cursor != 0)
                    {
                        cursor -= 1;
                        into.erase(cursor);
                    }
                    return -1;
                case '\x04': // validate
                    return 1;
                case '\x05': // first
                    cursor = 0;
                    return 0;
                case '\x06':
                    cursor = into.size();
                    return 0;
                default:
                    return 0;
                }
            }
            else
            {
                into.insert(into.begin() + cursor, c);
                cursor += 1;
                if((shift_state & 0x5) == 1)
                {
                    shift_state ^= 1;
                }
                return -1;
            }
        };

        for(const auto& k : get_active_pane())
        {
            if(k.x <= x && x < (k.x + k.w) && k.y <= y && y < (k.y + BUTTON_H))
            {
                return do_action(k.symbol);
            }
        }
        return 0;
    }
    const pane& get_active_pane() const
    {
        return panes[shift_state & 3];
    }
    keyboard()
        : shift_state(0)
    {
        {
        pane cur_pane;
        const char row1[] = "qwertyuiop";
        const char row2[] = "asdfghjkl";
        const char row3[] = "zxcvbnm";
        std::span<const char> rows[] = {
            row1, row2, row3
        };
        int y = 112;
        for(auto row : rows)
        {
            int x = (320 - 32 * (row.size() - 1)) / 2;
            for(auto c : row)
            {
                if(c == '\0')
                    continue;

                cur_pane.emplace_back(x+ 1, y + 1, 32 - 2, c);
                x += 32;
            }
            y += 32;
        }
        cur_pane.emplace_back(2, y + 1 - 32, 48 - 2, '\x01');
        cur_pane.emplace_back(2, y + 1, 48 - 2, '\x02');
        cur_pane.emplace_back((320 - 180) / 2 + 1, y + 1, 180 - 2, ' ');
        cur_pane.emplace_back(271, y + 1 - 32, 48 - 2, '\x03');
        cur_pane.emplace_back(271, y + 1, 48 - 2, '\x04');
        panes.push_back(std::move(cur_pane));
        }
        {
        pane cur_pane;
        const char row1[] = "QWERTYUIOP";
        const char row2[] = "ASDFGHJKL";
        const char row3[] = "ZXCVBNM";
        std::span<const char> rows[] = {
            row1, row2, row3
        };
        int y = 112;
        for(auto row : rows)
        {
            int x = (320 - 32 * (row.size() - 1)) / 2;
            for(auto c : row)
            {
                if(c == '\0')
                    continue;

                cur_pane.emplace_back(x+ 1, y + 1, 32 - 2, c);
                x += 32;
            }
            y += 32;
        }
        cur_pane.emplace_back(2, y + 1 - 32, 48 - 2, '\x01');
        cur_pane.emplace_back(2, y + 1, 48 - 2, '\x02');
        cur_pane.emplace_back((320 - 180) / 2 + 1, y + 1, 180 - 2, ' ');
        cur_pane.emplace_back(271, y + 1 - 32, 48 - 2, '\x03');
        cur_pane.emplace_back(271, y + 1, 48 - 2, '\x04');
        panes.push_back(std::move(cur_pane));
        }
        {
        pane cur_pane;
        const char row1[] = "1234567890";
        const char row2[] = "`-=\\[];";
        const char row3[] = "\x05',./\x06";
        std::span<const char> rows[] = {
            row1, row2, row3
        };
        int y = 112;
        for(auto row : rows)
        {
            int x = (320 - 32 * (row.size() - 1)) / 2;
            for(auto c : row)
            {
                if(c == '\0')
                    continue;

                cur_pane.emplace_back(x+ 1, y + 1, 32 - 2, c);
                x += 32;
            }
            y += 32;
        }
        cur_pane.emplace_back(2, y + 1 - 32, 48 - 2, '\x01');
        cur_pane.emplace_back(2, y + 1, 48 - 2, '\x02');
        cur_pane.emplace_back((320 - 180) / 2 + 1, y + 1, 180 - 2, ' ');
        cur_pane.emplace_back(271, y + 1 - 32, 48 - 2, '\x03');
        cur_pane.emplace_back(271, y + 1, 48 - 2, '\x04');
        panes.push_back(std::move(cur_pane));
        }
        {
        pane cur_pane;
        const char row1[] = "!@#$%^&*()";
        const char row2[] = "~_+|{}:";
        const char row3[] = "\x05\"<>?\x06";
        std::span<const char> rows[] = {
            row1, row2, row3
        };
        int y = 112;
        for(auto row : rows)
        {
            int x = (320 - 32 * (row.size() - 1)) / 2;
            for(auto c : row)
            {
                if(c == '\0')
                    continue;

                cur_pane.emplace_back(x+ 1, y + 1, 32 - 2, c);
                x += 32;
            }
            y += 32;
        }
        cur_pane.emplace_back(2, y + 1 - 32, 48 - 2, '\x01');
        cur_pane.emplace_back(2, y + 1, 48 - 2, '\x02');
        cur_pane.emplace_back((320 - 180) / 2 + 1, y + 1, 180 - 2, ' ');
        cur_pane.emplace_back(271, y + 1 - 32, 48 - 2, '\x03');
        cur_pane.emplace_back(271, y + 1, 48 - 2, '\x04');
        panes.push_back(std::move(cur_pane));
        }
    }
};
struct history {
    static constexpr inline unsigned HIST_SIZE = 14;
    struct element {
        std::string value{};
        C2D_Text text{};
        bool input{false};
        bool cont{false};
    };
    unsigned tail;
    std::array<element, HIST_SIZE> elems;
    history(C2D_TextBuf buf_arg, C2D_Font fnt_arg)
        : buf(buf_arg)
        , fnt(fnt_arg)
    {
        tail = 0;
        elems[tail].input = true;
    }

    void move_up()
    {
        if(tail == (HIST_SIZE - 1))
        {
            std::rotate(elems.begin(), elems.begin() + 1, elems.end());
            auto& b = elems.back();
            b.value.clear();
            b.cont = false;
            b.input = false;
        }
        else
        {
            tail += 1;
        }
    }
    void update_text()
    {
        C2D_TextBufClear(buf);
        for(auto& el : elems)
        {
            if(el.value.empty())
                continue;
            C2D_TextFontParse(&el.text, fnt, buf, el.value.c_str());
            C2D_TextOptimize(&el.text);
        }
    }

private:
    C2D_TextBuf buf;
    C2D_Font fnt;
};

int main(int argc, char **argv)
{
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    consoleDebugInit(debugDevice_SVC);

    C3D_RenderTarget* top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget* bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    romfsInit();
    C2D_Font mono_font = C2D_FontLoad("romfs:/gfx/NotoSansMono-Medium.bcfnt");
    C2D_TextBuf keyboard_tbuf = C2D_TextBufNew(512);
    C2D_TextBuf screen_tbuf = C2D_TextBufNew(2048);
    C2D_TextBuf static_tbuf = C2D_TextBufNew(128);
    C2D_SpriteSheet sprites = C2D_SpriteSheetLoad("romfs:/gfx/sprites.t3x");
    C2D_Image left_img = C2D_SpriteSheetGetImage(sprites, 0);
    C2D_Image right_img = C2D_SpriteSheetGetImage(sprites, 1);
    C2D_Image mid_img = C2D_SpriteSheetGetImage(sprites, 2);
    C2D_Image shift_off_img = C2D_SpriteSheetGetImage(sprites, 3);
    C2D_Image shift_on_img = C2D_SpriteSheetGetImage(sprites, 4);
    C2D_Image shift_full_img = C2D_SpriteSheetGetImage(sprites, 5);
    C2D_Image bsp_img = C2D_SpriteSheetGetImage(sprites, 6);
    C2D_Image sym_img = C2D_SpriteSheetGetImage(sprites, 7);
    C2D_Image txt_img = C2D_SpriteSheetGetImage(sprites, 8);
    C2D_Image send_img = C2D_SpriteSheetGetImage(sprites, 9);
    C2D_Image first_img = C2D_SpriteSheetGetImage(sprites, 10);
    C2D_Image last_img = C2D_SpriteSheetGetImage(sprites, 11);

    int retval = 0;
    {
    python_handler handler;
    ctr::thread::meta meta = ctr::thread::basic_meta;
    meta.stack_size = 80 * 1024;
    meta.prio += 1;
    ctr::thread python_handler_th(meta, &python_handler::loop_func, &handler);
    Printer::payload = &handler;
    Printer::callback = &python_handler::print_callback;

    C2D_Text input_txt, cont_txt;
    C2D_TextFontParse(&input_txt, mono_font, static_tbuf, ">>>");
    C2D_TextFontParse(&cont_txt, mono_font, static_tbuf, "...");
    C2D_TextOptimize(&input_txt);
    C2D_TextOptimize(&cont_txt);

    C2D_ImageTint green_tint;
    C2D_PlainImageTint(&green_tint, C2D_Color32(0,160,0,255), 1.0f);

    keyboard keeb;
    std::string final_upload;
    history hist(screen_tbuf, mono_font);

    unsigned cursor = 0;
    unsigned framecnt = 0;
    touchPosition init_touch, last_touch;
    bool last_line_done = true;

    auto get_current_input = [&]() -> std::string& {
        return hist.elems[hist.tail].value;
    };
    auto do_publish_line = [&]()
    {
        if(!final_upload.empty())
            final_upload += '\n';
        final_upload += get_current_input();
        if(mp_repl_continue_with_input(final_upload.c_str()))
        {
            cursor = 0;
            hist.move_up();
            hist.elems[hist.tail].input = true;
            hist.elems[hist.tail].cont = true;
        }
        else
        {
            handler.write(final_upload);
            final_upload.clear();
            cursor = 0;
            hist.move_up();
            last_line_done = false;
        }
    };
    
    while(aptMainLoop())
    {
        hidScanInput();

        // Your code goes here
        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();
        u32 kUp = hidKeysUp();
        const bool line_done = handler.is_line_done();
        while(auto r = handler.read())
        {
            auto& s = *r;
            std::size_t pos = 0;
            std::string_view sv(s);
            do {
                pos = sv.find_first_of("\r\n");
                if(pos == std::string_view::npos)
                {
                    get_current_input() += sv;
                }
                else if(sv[pos] == '\r')
                {
                    if(pos != 0)
                    {
                        get_current_input() += sv.substr(0, pos);
                    }
                    sv.remove_prefix(pos + 1);
                }
                else
                {
                    if(pos != 0)
                    {
                        get_current_input() += sv.substr(0, pos);
                    }
                    sv.remove_prefix(pos + 1);
                    hist.move_up();
                }
            } while(pos != std::string_view::npos);
            hist.update_text();
        }
        if(!last_line_done && line_done)
        {
            if(auto r = handler.should_exit(); r)
            {
                retval = *r;
                break;
            }
            hist.elems[hist.tail].input = true;
        }
        if(last_line_done != line_done)
        {
            last_line_done = line_done;
        }
        if(kDown & KEY_TOUCH)
        {
            hidTouchRead(&init_touch);
            hidTouchRead(&last_touch);
        }
        else if(kHeld & KEY_TOUCH)
        {
            hidTouchRead(&last_touch);
        }
        else if(kUp & KEY_TOUCH)
        {
            if(line_done && std::hypot(last_touch.px - init_touch.px, last_touch.py - init_touch.py) < 24.0)
            {
                switch(keeb.do_press(last_touch.px, last_touch.py, get_current_input(), cursor))
                {
                case 1:
                    do_publish_line();
                    break;
                case -1:
                    hist.update_text();
                    break;
                default:
                    break;
                }
            }
        }

        if(kDown & KEY_DLEFT)
        {
            if(cursor != 0)
            {
                cursor -= 1;
            }
        }
        else if(kDown & KEY_DRIGHT)
        {
            if(cursor != get_current_input().size())
            {
                cursor += 1;
            }
        }

        if(line_done && kDown & KEY_A)
        {
            do_publish_line();
        }
        else if(kDown & KEY_B)
        {
            if(line_done)
            {
                if(cursor != 0)
                {
                    cursor -= 1;
                    get_current_input().erase(cursor);
                    hist.update_text();
                }
            }
            else
            {
                handler.signal_interrupt();
            }
        }

        C2D_TextBufClear(keyboard_tbuf);

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

        C2D_TargetClear(top, C2D_Color32(0,0,0,255));
        C2D_TargetClear(bottom, C2D_Color32(0,0,0,255));

        C2D_SceneBegin(top);

        {
        int y = 0;
        unsigned i = 0;
        for(auto& el : hist.elems)
        {
            float x = 0.0f;
            if(el.input)
            {
                C2D_Text* txt = el.cont ? &cont_txt : &input_txt;
                C2D_DrawText(txt, C2D_WithColor, x, y, 0.0f, 0.75f, 0.75f, C2D_Color32(0, 255, 0, 255));
                C2D_TextGetDimensions(txt, 0.75f, 0.75f, &x, nullptr);
            }
            if(!el.value.empty())
            {
                C2D_DrawText(&el.text, C2D_WithColor, x, y, 0.0f, 0.75f, 0.75f, C2D_Color32(0, 255, 0, 255));
            }
            if(line_done && el.input && hist.tail == i && framecnt < 20)
            {
                C2D_DrawRectSolid(x + 8.5f * cursor + 1.0f, y + 3, 1.0f, 2.0f, (240/history::HIST_SIZE), C2D_Color32(0, 255, 0, 255));
            }
            y += 240/history::HIST_SIZE;
            ++i;
        }
        }

        C2D_SceneBegin(bottom);
        {
        char buf[2] = {0};
        for(const auto& el : keeb.get_active_pane())
        {
            C2D_DrawImageAt(left_img, el.x, el.y, 0.0f);
            C2D_DrawImageAt(right_img, el.x + el.w - right_img.subtex->width, el.y, 0.0f);
            C2D_DrawImageAt(mid_img, el.x + left_img.subtex->width, el.y, 0.0f, nullptr, (el.w - left_img.subtex->width - right_img.subtex->width), 1.0f);
            if(el.symbol < 0x20)
            {
                switch(el.symbol)
                {
                case 1:
                    {
                    C2D_Image* im = nullptr;
                    switch(keeb.shift_state & 5)
                    {
                    case 0:
                        im = &shift_off_img;
                        break;
                    case 1:
                        im = &shift_on_img;
                        break;
                    case 5:
                        im = &shift_full_img;
                        break;
                    default:
                        im = &shift_off_img;
                        break;
                    }
                    C2D_DrawImageAt(*im, el.x + (el.w - im->subtex->width) * 0.5f, el.y+ (keyboard::DRAWN_BUTTON_H - im->subtex->height) * 0.5f, 0.5f, &green_tint);
                    }
                    break;
                case 2:
                    {
                    const auto im = keeb.shift_state & 2 ? txt_img : sym_img;
                    C2D_DrawImageAt(im, el.x + (el.w - im.subtex->width) * 0.5f, el.y+ (keyboard::DRAWN_BUTTON_H - im.subtex->height) * 0.5f, 0.5f, &green_tint);
                    }
                    break;
                case 3:
                    C2D_DrawImageAt(bsp_img, el.x + (el.w - bsp_img.subtex->width) * 0.5f, el.y+ (keyboard::DRAWN_BUTTON_H - bsp_img.subtex->height) * 0.5f, 0.5f, &green_tint);
                    break;
                case 4:
                    C2D_DrawImageAt(send_img, el.x + (el.w - send_img.subtex->width) * 0.5f, el.y+ (keyboard::DRAWN_BUTTON_H - send_img.subtex->height) * 0.5f, 0.5f, &green_tint);
                    break;
                case 5:
                    C2D_DrawImageAt(first_img, el.x + (el.w - first_img.subtex->width) * 0.5f, el.y+ (keyboard::DRAWN_BUTTON_H - first_img.subtex->height) * 0.5f, 0.5f, &green_tint);
                    break;
                case 6:
                    C2D_DrawImageAt(last_img, el.x + (el.w - last_img.subtex->width) * 0.5f, el.y+ (keyboard::DRAWN_BUTTON_H - last_img.subtex->height) * 0.5f, 0.5f, &green_tint);
                    break;
                }
            }
            else if(el.symbol != 0x20)
            {
                C2D_Text txt;
                buf[0] = el.symbol;
                C2D_TextFontParse(&txt, mono_font, keyboard_tbuf, buf);
                float w = 0.0f, h = 0.0f;
                C2D_TextGetDimensions(&txt, 1.0f, 1.0f, &w, &h);
                C2D_DrawText(&txt, C2D_WithColor, el.x + (el.w - w) * 0.5f, el.y + (keyboard::DRAWN_BUTTON_H - h) * 0.5f, 0.5f, 1.0f, 1.0f, C2D_Color32(0, 160, 0, 255));
            }
        }
        }


        C3D_FrameEnd(0);
        framecnt = (framecnt + 1) % 40;
    }
    handler.signal_stop();
    python_handler_th.join();
    }

    C2D_SpriteSheetFree(sprites);
    C2D_TextBufDelete(keyboard_tbuf);
    C2D_TextBufDelete(screen_tbuf);
    C2D_TextBufDelete(static_tbuf);
    C2D_FontFree(mono_font);

    romfsExit();

    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return retval;
}
