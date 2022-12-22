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
    , current_mode(mode::repl)
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
}

void application::press_key(std::string_view key, bool repeat=false)
{
    if(currently() == mode::repl)
    {
        if(key == "\n")
        {
            if(hist.is_hovering())
            {
                hist.copy_to_current();
            }
            send_repl_line();
        }
        else
        {
            scr.print(key);
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
    if(currently() == mode::repl && std::hypot(last_click_x - start_click_x, last_click_y - start_click_y) < 15.0)
    {
        auto callback = [&](const char c) {
            if(hist.is_hovering())
            {
                hist.copy_to_current();
            }
            std::string& into = hist.get_current();
            const std::size_t pos_x = (scr.cursor_x + scr.scroll_x);
            if(c == '\x08')
            {
                into.erase(pos_x, 1);
                std::string_view sv(&c, 1);
                scr.print(sv);
            }
            else if(c == '\x00')
            {
                scr.cursor_x = std::min(screen::COLS - 1, into.size());
                scr.scroll_x = into.size() >= screen::COLS ? into.size() - scr.cursor_x : 0;
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
                    const std::size_t sx = scr.cursor_x;
                    scr.print(sv.substr(pos_x + 1, screen::COLS - cx - 1));
                    scr.cursor_x = cx;
                    scr.cursor_x = sx;
                }
                else // cursor + scroll longer than string, shouldn't happen
                {
                    fprintf(stderr, "wtf - %s:%d\n", __FILE__, __LINE__);
                }
            }
        };

        if(keeb.do_press(last_click_x, last_click_y, callback))
        {
            if(hist.is_hovering())
            {
                hist.copy_to_current();
            }
            send_repl_line();
        }
    }
}


void application::send_repl_line()
{
    if(!final_upload.empty())
    {
        final_upload += '\n';
    }
    final_upload += hist.get_current();
    if(mp_repl_continue_with_input(final_upload.c_str()))
    {
        
    }
    else
    {
        handler.write(final_upload);
        final_upload.clear();
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
    while((current_read_status = handler.read(current_read)) == 1 && --up_to)
    {
        scr.print(current_read);
    }
    
    if(current_read_status == 0 && currently() == mode::waiting)
    {
        set_mode(mode::repl);
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

}

void application::set_mode(application::mode m)
{

}
