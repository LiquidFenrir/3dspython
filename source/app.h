#pragma once

#include "keyboard.h"
#include "screen.h"
#include "history.h"
#include "python_handler.h"

struct application {
    enum class mode {
        waiting,
        editing,
        repl,
    };

    application(C2D_Font fnt, C2D_SpriteSheet sprites);

    void press_key(std::string_view key, bool repeat=false);
    void click_start_at(int x, int y);
    void click_move_to(int x, int y);
    void click_release();

    void tick();
    void read_output(unsigned up_to);
    std::optional<int> return_value() const;

    void set_keyboard_color(u32 color);
    void draw_top();
    void draw_bottom();

    mode currently() const;
    void set_mode(mode);

private:
    python_handler handler;
    screen scr;
    keyboard keeb;
    history hist;

    void send_repl_line();
    void start_repl_line(bool is_cont);

    void typing_callback_repl(const char c);

    int start_click_x, start_click_y;
    int last_click_x, last_click_y;

    std::string final_upload;
    C2D_TextBuf keyboard_tbuf;
    C2D_Font mono_font;
    u32 keyboard_color;
    C2D_ImageTint keyboard_sprite_tint;
    mode current_mode;
    C2D_Image left_img;
    C2D_Image right_img;
    C2D_Image mid_img;
    C2D_Image shift_off_img;
    C2D_Image shift_on_img;
    C2D_Image shift_full_img;
    C2D_Image bsp_img;
    C2D_Image sym_img;
    C2D_Image txt_img;
    C2D_Image send_img;
    C2D_Image first_img;
    C2D_Image last_img;
};
