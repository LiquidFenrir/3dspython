#include "app.h"

extern "C" {
#include "py/repl.h"
}

static std::string_view import_search_paths[] = {
    "sdmc:/python-lib",
    "sdmc:/python-work",
};

application::application(C2D_Font fnt, C2D_SpriteSheet sprites)
    : handler(import_search_paths)
    , scr(fnt)
    , keyboard_tbuf(C2D_TextBufNew(512))
    , mono_font(fnt)
{
    set_keyboard_color(C2D_Color32(0,172,0,255));

    left_img = C2D_SpriteSheetGetImage(sprites, 0);
    right_img = C2D_SpriteSheetGetImage(sprites, 1);
    mid_img = C2D_SpriteSheetGetImage(sprites, 2);
    shift_off_img = C2D_SpriteSheetGetImage(sprites, 3);
    shift_on_img = C2D_SpriteSheetGetImage(sprites, 4);
    shift_full_img = C2D_SpriteSheetGetImage(sprites, 5);
    bsp_img = C2D_SpriteSheetGetImage(sprites, 6);
    sym_img = C2D_SpriteSheetGetImage(sprites, 7);
    txt_img = C2D_SpriteSheetGetImage(sprites, 8);
    send_img = C2D_SpriteSheetGetImage(sprites, 9);
    first_img = C2D_SpriteSheetGetImage(sprites, 10);
    last_img = C2D_SpriteSheetGetImage(sprites, 11);

    start_repl_line(false);
}

void application::press_key(std::string_view key, bool repeat)
{
    if(currently() == mode::repl)
    {
        if(key == "\n")
        {
            send_repl_line();
        }
        else if(key == "\x08")
        {
            if(hist.is_hovering())
            {
                hist.copy_to_current();
            }
            auto& into = hist.get_current();
            if(!into.empty() && (scr.cursor_x + scr.scroll_x - 4) != 0)
            {
                const std::size_t idx = scr.cursor_x + scr.scroll_x - 4 - 1;
                into.erase(idx, 1);
                scr.print(key);
            }
        }
        else if(key == "\e[A")
        {
            hist.get_previous();
            if(hist.is_hovering())
            {
                scr.cursor_x = 4;
                scr.scroll_x = 0;
                scr.print("\e[K");
                scr.print(hist.get_hover());
            }
        }
        else if(key == "\e[B")
        {
            if(hist.is_hovering())
            {
                hist.get_next();
                scr.cursor_x = 4;
                scr.scroll_x = 0;
                scr.print("\e[K");
                scr.print(hist.get_hover());
            }
        }
        else if(key == "\e[D")
        {
            if(scr.cursor_x + scr.scroll_x != 4)
            {
                scr.print(key);
            }
        }
        else if(key == "\e[C")
        {
            const auto& into = hist.is_hovering() ? hist.get_hover() : hist.get_current();
            if((scr.cursor_x + scr.scroll_x) != into.size() + 4)
            {
                scr.print(key);
            }
        }
        else
        {
            for(const auto c : key)
            {
                typing_callback_repl(c);
            }
        }
    }
    else if(currently() == mode::waiting)
    {
        if(key == "\x08" && !repeat)
        {
            handler.signal_interrupt();
        }
    }
}

void application::click_start_at(int x, int y)
{
    start_click_x = x;
    start_click_y = y;
}

void application::click_move_to(int x, int y)
{
    last_click_x = x;
    last_click_y = y;
}

void application::click_release()
{
    if(std::hypot(last_click_x - start_click_x, last_click_y - start_click_y) < 15.0)
    {
        switch(currently())
        {
        case mode::repl:
            if(keeb.do_press(last_click_x, last_click_y, [&](const char c) { typing_callback_repl(c); }))
            {
                send_repl_line();
            }
            break;
        default:
            break;
        }
    }
}

void application::send_repl_line()
{
    if(hist.is_hovering())
    {
        hist.copy_to_current();
    }
    if(!final_upload.empty())
    {
        final_upload += '\n';
    }
    final_upload += hist.get_current();
    hist.validate();
    scr.print("\n");
    if(mp_repl_continue_with_input(final_upload.c_str()))
    {
        start_repl_line(true);
    }
    else
    {
        handler.write(final_upload);
        scr.print("\e[25m");
        final_upload.clear();
        set_mode(mode::waiting);
    }
}

void application::start_repl_line(bool is_cont)
{
    set_mode(mode::repl);
    scr.print("\e[0m");
    scr.print(is_cont ? "... " : ">>> ");
}

void application::typing_callback_repl(const char c)
{
    if(hist.is_hovering())
    {
        hist.copy_to_current();
    }
    std::string& into = hist.get_current();
    const std::size_t pos_x = (scr.cursor_x + scr.scroll_x) - 4;
    if(c == '\x08')
    {
        if(!into.empty() && pos_x != 0)
        {
            const std::size_t idx = pos_x - 1;
            into.erase(idx, 1);
            std::string_view sv(&c, 1);
            scr.print(sv);
        }
    }
    else if(c == '\x00')
    {
        scr.cursor_x = std::min(scr.columns() - 1, into.size());
        scr.scroll_x = into.size() >= scr.columns() ? into.size() - scr.cursor_x : 0;
    }
    else if(c == '\r')
    {
        std::string_view sv(&c, 1);
        scr.print(sv);
    }
    else
    {
        if(pos_x == into.size())
        {
            into.push_back(c);
            std::string_view sv(&c, 1);
            scr.print(sv);
        }
        else if(pos_x < into.size())
        {
            into.insert(into.begin() + pos_x, c);
            std::string_view sv(&c, 1);
            scr.print(sv);
            sv = into;
            const std::size_t cx = scr.cursor_x;
            const std::size_t sx = scr.scroll_x;
            scr.print(sv.substr(pos_x + 1, scr.columns() - cx - 1));
            scr.cursor_x = cx;
            scr.scroll_x = sx;
        }
        else // cursor + scroll longer than string, shouldn't happen
        {
            fprintf(stderr, "wtf - %s:%d\n", __FILE__, __LINE__);
        }
    }
}

void application::tick()
{
    scr.tick();
}

void application::read_output(unsigned up_to)
{
    std::string current_read;
    int current_read_status = 0;
    while(--up_to && (current_read_status = handler.read(current_read)) == 1)
    {
        std::string_view sv(current_read);
        while((sv.size() + scr.cursor_x + scr.scroll_x) >= scr.columns())
        {
            auto sub = sv.substr(0, scr.columns() - (scr.cursor_x + scr.scroll_x));
            scr.print(sub);
            scr.print("\n");
            sv.remove_prefix(sub.size());
        }
        scr.print(sv);
    }

    if(up_to && current_read_status == 0 && currently() == mode::waiting)
    {
        start_repl_line(false);
    }
}

std::optional<int> application::return_value() const
{
    return handler.should_exit();
}

void application::set_keyboard_color(u32 color)
{
    keyboard_color = color;
    C2D_PlainImageTint(&keyboard_sprite_tint, color, 1.0f);
}

void application::draw_top()
{
    scr.draw();
}

void application::draw_bottom()
{
    C2D_TextBufClear(keyboard_tbuf);

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
                C2D_DrawImageAt(*im, el.x + (el.w - im->subtex->width) * 0.5f, el.y+ (keyboard::DRAWN_BUTTON_H - im->subtex->height) * 0.5f, 0.5f, &keyboard_sprite_tint);
                }
                break;
            case 2:
                {
                const auto im = keeb.shift_state & 2 ? txt_img : sym_img;
                C2D_DrawImageAt(im, el.x + (el.w - im.subtex->width) * 0.5f, el.y+ (keyboard::DRAWN_BUTTON_H - im.subtex->height) * 0.5f, 0.5f, &keyboard_sprite_tint);
                }
                break;
            case 3:
                C2D_DrawImageAt(bsp_img, el.x + (el.w - bsp_img.subtex->width) * 0.5f, el.y+ (keyboard::DRAWN_BUTTON_H - bsp_img.subtex->height) * 0.5f, 0.5f, &keyboard_sprite_tint);
                break;
            case 4:
                C2D_DrawImageAt(send_img, el.x + (el.w - send_img.subtex->width) * 0.5f, el.y+ (keyboard::DRAWN_BUTTON_H - send_img.subtex->height) * 0.5f, 0.5f, &keyboard_sprite_tint);
                break;
            case 5:
                C2D_DrawImageAt(first_img, el.x + (el.w - first_img.subtex->width) * 0.5f, el.y+ (keyboard::DRAWN_BUTTON_H - first_img.subtex->height) * 0.5f, 0.5f, &keyboard_sprite_tint);
                break;
            case 6:
                C2D_DrawImageAt(last_img, el.x + (el.w - last_img.subtex->width) * 0.5f, el.y+ (keyboard::DRAWN_BUTTON_H - last_img.subtex->height) * 0.5f, 0.5f, &keyboard_sprite_tint);
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

application::mode application::currently() const
{
    return current_mode;
}

void application::set_mode(application::mode m)
{
    current_mode = m;
}
