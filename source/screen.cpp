#include "screen.h"
#include <algorithm>

screen::screen(C2D_Font fnt_arg)
    : fnt(fnt_arg)
{
    C2D_TextBuf buf = C2D_TextBufNew(2);
    C2D_Text txt;
    C2D_TextFontParse(&txt, fnt_arg, buf, "O");
    C2D_TextGetDimensions(&txt, TEXT_SCALE, TEXT_SCALE, &charW, &charH);
    C2D_TextBufDelete(buf);

    flags = 0;
    bg = FIXED_COLOR_TABLE[DEFAULT_BG_IDX];
    fg = FIXED_COLOR_TABLE[DEFAULT_FG_IDX];

    for(auto& el : row_elems)
    {
        el.buf = C2D_TextBufNew(COLS);
    }
}
screen::~screen()
{
    for(auto& el : row_elems)
    {
        C2D_TextBufDelete(el.buf);
    }
}

void screen::print(std::string_view str)
{
    while(!str.empty())
    {
        if(str[0] == '\e' && str.size() > 1 && str[1] == '[')
        {
            str.remove_prefix(2);
            bool escaping = true;
            std::string_view escapeseq = str;
            do {
                const char chr = str.front();
                str.remove_prefix(1);
                if((chr >= '0' && chr <= '9') || chr == ';')
                    continue;
                
                switch(chr)
                {
                case 'A':
                    {
                    const unsigned parameter = escapeseq.front() == 'A' ? 1 : readStr<unsigned>(escapeseq);
                    cursor_y  =  (cursor_y  - parameter) < 0 ? 0 : (cursor_y  - parameter);
                    scroll_x = 0;
                    escaping = false;
                    }
                    break;
                case 'B':
                    {
                    const unsigned parameter = escapeseq.front() == 'B' ? 1 : readStr<unsigned>(escapeseq);
                    cursor_y  =  (cursor_y + parameter) > (COLS - 1) ? (COLS - 1) : (cursor_y  + parameter);
                    scroll_x = 0;
                    escaping = false;
                    }
                    break;
                case 'C':
                    {
                    const unsigned parameter = escapeseq.front() == 'C' ? 1 : readStr<unsigned>(escapeseq);
                    if((cursor_x  + parameter) > (COLS - 1))
                    {
                        const auto old_x = cursor_x;
                        cursor_x = (COLS - 1);
                        scroll_x += parameter - (cursor_x - old_x);
                    }
                    else
                    {
                        cursor_x += parameter;
                    }
                    escaping = false;
                    }
                    break;
                case 'D':
                    {
                    const unsigned parameter = escapeseq.front() == 'D' ? 1 : readStr<unsigned>(escapeseq);
                    if((cursor_x - parameter) < 0)
                    {
                        const auto old_x = cursor_x;
                        cursor_x = 0;
                        if(scroll_x < (parameter - old_x))
                        {
                            scroll_x = 0;
                        }
                        else
                        {
                            scroll_x -= parameter - old_x;
                        }
                    }
                    else
                    {
                        cursor_x -= parameter;
                    }
                    escaping = false;
                    }
                    break;
                case 'S': // scroll up
                    {
                    const unsigned parameter = escapeseq.front() == 'S' ? 1 : readStr<unsigned>(escapeseq);
                    if(parameter < row_elems.size() && parameter)
                    {
                        std::rotate(row_elems.begin(), row_elems.begin() + parameter, row_elems.end());
                    }
                    unsigned i = 0;
                    for(auto it = row_elems.rbegin(), fin = row_elems.rend(); i < parameter && it != fin; ++i, ++it)
                    {
                        auto& e = *it;
                        e.value.clear();
                        e.updated = true;
                    }
                    escaping = false;
                    }
                    break;
                case 'T': // scroll down
                    {
                    const unsigned parameter = escapeseq.front() == 'T' ? 1 : readStr<unsigned>(escapeseq);
                    if(parameter < row_elems.size() && parameter)
                    {
                        std::rotate(row_elems.rbegin(), row_elems.rbegin() + parameter, row_elems.rend());
                    }
                    unsigned i = 0;
                    for(auto it = row_elems.begin(), fin = row_elems.end(); i < parameter && it != fin; ++i, ++it)
                    {
                        auto& e = *it;
                        e.value.clear();
                        e.updated = true;
                    }
                    escaping = false;
                    }
                    break;
                case 'H':
                case 'f':
                {
                    unsigned x, y;
                    if(escapeseq.front() == ';' ) // no y provided
                    {
                        y = 1;
                        escapeseq.remove_prefix(1);
                    }
                    else
                    {
                        y = readStr<unsigned>(escapeseq);
                        escapeseq.remove_prefix(1); // remove ';'
                    }

                    if(escapeseq.front() == chr) // no x provided
                    {
                        x = 1;
                    }
                    else
                    {
                        x = readStr<unsigned>(escapeseq);
                    }

                    if(y == 0)
                        y = 1;
                    if(x == 0)
                        x = 1;

                    cursor_y = y - 1;
                    cursor_x = x - 1;
                    scroll_x = 0;

                    escaping = false;
                    break;
                }
                //---------------------------------------
                // Screen clear
                //---------------------------------------
                case 'J':
                    switch(escapeseq.front())
                    {
                    case 'J':
                    case '0':
                        // cursor -> EOS
                        {
                            for(std::size_t y = cursor_y; y < ROWS; ++y)
                            {
                                if(y == cursor_y)
                                {
                                    row_elems[y].value.erase(cursor_x + scroll_x);
                                    std::size_t x = 0;
                                    for(auto& c : row_elems[y].chars)
                                    {
                                        if(x++ >= cursor_x)
                                        {
                                            c.bg = bg;
                                            c.fg = fg;
                                        }
                                    }
                                }
                                else
                                {
                                    row_elems[y].value.clear();
                                    for(auto& c : row_elems[y].chars)
                                    {
                                        c.bg = bg;
                                        c.fg = fg;
                                    }
                                }
                                row_elems[y].updated = true;
                            }
                        }
                        break;
                    case '1':
                        // BOS -> cursor
                        for(std::size_t y = 0; y <= cursor_y; ++y)
                        {
                            if(y == cursor_y)
                            {
                                if(cursor_x + scroll_x)
                                {
                                    row_elems[y].value.erase(0, cursor_x + scroll_x);
                                    std::size_t x = 0;
                                    for(auto& c : row_elems[y].chars)
                                    {
                                        if(x++ < cursor_x)
                                        {
                                            c.bg = bg;
                                            c.fg = fg;
                                        }
                                    }
                                }
                            }
                            else
                            {
                                for(auto& c : row_elems[y].chars)
                                {
                                    c.bg = bg;
                                    c.fg = fg;
                                }
                                row_elems[y].value.clear();
                            }
                            row_elems[y].updated = true;
                        }
                        break;
                    case '2':
                        // whole screen
                        for(std::size_t y = 0; y < ROWS; ++y)
                        {
                            row_elems[y].value.clear();
                            for(auto& c : row_elems[y].chars)
                            {
                                c.bg = bg;
                                c.fg = fg;
                            }
                        }
                        cursor_x = 0;
                        cursor_y = 0;
                        break;
                    }
                    escaping = false;
                    break;
                //---------------------------------------
                // Line clear
                //---------------------------------------
                case 'K':
                    switch(escapeseq.front())
                    {
                    case 'K':
                    case '0':
                        // cursor -> EOL
                        {
                        row_elems[cursor_y].value.erase(cursor_x + scroll_x);
                        std::size_t x = 0;
                        for(auto& c : row_elems[cursor_y].chars)
                        {
                            if(x++ >= cursor_x)
                            {
                                c.bg = bg;
                                c.fg = fg;
                            }
                        }
                        }
                        break;
                    case '1':
                        // BOL -> cursor
                        {
                        row_elems[cursor_y].value.erase(0, cursor_x + scroll_x);
                        scroll_x = 0;
                        std::size_t x = 0;
                        for(auto& c : row_elems[cursor_y].chars)
                        {
                            if(x++ < cursor_x)
                            {
                                c.bg = bg;
                                c.fg = fg;
                            }
                        }
                        }
                        break;
                    case '2':
                        // whole line
                        row_elems[cursor_y].value.clear();
                        scroll_x = 0;
                        for(auto& c : row_elems[cursor_y].chars)
                        {
                            c.bg = bg;
                            c.fg = fg;
                        }
                        break;
                    }
                    escaping = false;
                    break;
                case 'm':
                    do {
                        unsigned code = 0;
                        if(escapeseq.front() != 'm')
                        {
                            code = readStr<unsigned>(escapeseq);
                            if(escapeseq.front() == ';')
                                escapeseq.remove_prefix(1);
                        }

                        switch(code)
                        {
                        case 0: // reset
                            flags = 0;
                            bg    = FIXED_COLOR_TABLE[DEFAULT_BG_IDX];
                            fg    = FIXED_COLOR_TABLE[1];
                            break;
                        case 1: // bold
                            flags &= ~CONSOLE_COLOR_FAINT;
                            flags |= CONSOLE_COLOR_BOLD;
                            break;

                        case 2: // faint
                            flags &= ~CONSOLE_COLOR_BOLD;
                            flags |= CONSOLE_COLOR_FAINT;
                            break;

                        case 3: // italic
                            flags |= CONSOLE_ITALIC;
                            break;

                        case 4: // underline
                            flags |= CONSOLE_UNDERLINE;
                            break;

                        case 5: // blink slow
                            flags &= ~CONSOLE_BLINK_FAST;
                            flags |= CONSOLE_BLINK_SLOW;
                            break;

                        case 6: // blink fast
                            flags &= ~CONSOLE_BLINK_SLOW;
                            flags |= CONSOLE_BLINK_FAST;
                            break;

                        case 7: // reverse video
                            flags |= CONSOLE_COLOR_REVERSE;
                            break;

                        case 8: // conceal
                            flags |= CONSOLE_CONCEAL;
                            break;

                        case 9: // crossed-out
                            flags |= CONSOLE_CROSSED_OUT;
                            break;

                        case 21: // bold off
                            flags &= ~CONSOLE_COLOR_BOLD;
                            break;

                        case 22: // normal color
                            flags &= ~CONSOLE_COLOR_BOLD;
                            flags &= ~CONSOLE_COLOR_FAINT;
                            break;

                        case 23: // italic off
                            flags &= ~CONSOLE_ITALIC;
                            break;

                        case 24: // underline off
                            flags &= ~CONSOLE_UNDERLINE;
                            break;

                        case 25: // blink off
                            flags &= ~CONSOLE_BLINK_SLOW;
                            flags &= ~CONSOLE_BLINK_FAST;
                            break;

                        case 27: // reverse off
                            flags &= ~CONSOLE_COLOR_REVERSE;
                            break;

                        case 29: // crossed-out off
                            flags &= ~CONSOLE_CROSSED_OUT;
                            break;

                        case 30 ... 37: // writing color
                            fg     = FIXED_COLOR_TABLE[code - 30];
                            break;
                        case 90 ... 97: // bright writing color
                            fg     = FIXED_COLOR_TABLE[code - 90 + 8];
                            break;
                        
                        case 38: // special writing color
                            if(escapeseq.front() == '2')
                            {
                                escapeseq.remove_prefix(2);
                                u8 r = 0, g = 0, b = 0;

                                if(escapeseq.front() != ';')
                                {
                                    r = readStr<unsigned>(escapeseq);
                                }
                                escapeseq.remove_prefix(1);
                                
                                if(escapeseq.front() != ';')
                                {
                                    g = readStr<unsigned>(escapeseq);
                                }
                                escapeseq.remove_prefix(1);

                                if(escapeseq.front() != ';')
                                {
                                    b = readStr<unsigned>(escapeseq);
                                }
                                if(escapeseq.front() == ';')
                                    escapeseq.remove_prefix(1);

                                fg = C2D_Color32(r,g,b,255);
                            }
                            else if(escapeseq.front() == '5')
                            {
                                escapeseq.remove_prefix(2);
                                const unsigned n = readStr<unsigned>(escapeseq);
                                fg = FIXED_COLOR_TABLE[n & 0xff];
                            }
                            else
                            {
                                // ???
                            }
                            break;

                        case 39: // reset foreground color
                            fg     = FIXED_COLOR_TABLE[DEFAULT_FG_IDX];
                            break;

                        case 40 ... 47: // screen color
                            bg = FIXED_COLOR_TABLE[code - 40];
                            break;
                        case 100 ... 107: // bright screen color
                            bg = FIXED_COLOR_TABLE[code - 100 + 8];
                            break;

                        case 48: // special screen color
                            if(escapeseq.front() == '2')
                            {
                                escapeseq.remove_prefix(2);
                                u8 r = 0, g = 0, b = 0;

                                if(escapeseq.front() != ';')
                                {
                                    r = readStr<unsigned>(escapeseq);
                                }
                                escapeseq.remove_prefix(1);
                                
                                if(escapeseq.front() != ';')
                                {
                                    g = readStr<unsigned>(escapeseq);
                                }
                                escapeseq.remove_prefix(1);

                                if(escapeseq.front() != ';')
                                {
                                    b = readStr<unsigned>(escapeseq);
                                }
                                if(escapeseq.front() == ';')
                                    escapeseq.remove_prefix(1);

                                bg = C2D_Color32(r,g,b,255);
                            }
                            else if(escapeseq.front() == '5')
                            {
                                escapeseq.remove_prefix(2);
                                const unsigned n = readStr<unsigned>(escapeseq);
                                bg = FIXED_COLOR_TABLE[n & 0xff];
                            }
                            else
                            {
                                // ???
                            }
                            break;

                        case 49: // reset background color
                            fg = FIXED_COLOR_TABLE[DEFAULT_BG_IDX];
                            break;
                        }
                    } while(escapeseq.front() != 'm');
                    escaping = false;
                    break;
                default:
                    // some sort of unsupported escape; just gloss over it
                    escaping = false;
                    break;
                }
            } while (escaping);
            continue;
        }

        printChar(str.front());
        str.remove_prefix(1);
    }
}
void screen::tick()
{
    if(flags & CONSOLE_BLINK_SLOW)
    {
        frame_counter += 1;
        if(frame_counter == 30)
        {
            frame_counter = 0;
            cursor_visible = !cursor_visible;
        }
    }
    else if(flags & CONSOLE_BLINK_FAST)
    {
        frame_counter += 1;
        if(frame_counter == 18)
        {
            frame_counter = 0;
            cursor_visible = !cursor_visible;
        }
    }
    else
    {
        frame_counter = 0;
    }

    u8 conversion_buf[5];

    for(std::size_t i = 0; i < ROWS; ++i)
    {
        auto& el = row_elems[i];
        if(!el.updated) continue;

        el.updated = false;
        std::u32string_view sv(el.value);
        if(i == cursor_y && scroll_x)
        {
            sv.remove_prefix(scroll_x);
        }
        std::size_t char_cnt = 0;
        C2D_TextBufClear(el.buf);
        for(const auto c : sv)
        {
            conversion_buf[encode_utf8(conversion_buf, c)] = 0;
            C2D_TextFontParse(&el.chars[char_cnt].text, fnt, el.buf, (const char*)conversion_buf);
            el.chars[char_cnt].have_fg = true;
            if(char_cnt++ == COLS)
                break;
        }
        for(; char_cnt < COLS; ++char_cnt)
        {
            el.chars[char_cnt].have_fg = false;
        }
    }
}
void screen::printChar(char32_t c)
{
    if(c == '\r')
    {
        cursor_x = 0;
        scroll_x = 0;
    }
    else if(c == '\n')
    {
        newLine();
    }
    else if(c == '\x08')
    {
        if((cursor_x + scroll_x) != 0)
        {
            row_elems[cursor_y].value.erase(cursor_x + scroll_x, 1);
            row_elems[cursor_y].updated = true;
            if(cursor_x == 0)
                scroll_x--;
            else
                cursor_x -= 1;
        }
    }
    else
    {
        auto& e = row_elems[cursor_y];
        auto& s = e.value;
        if((cursor_x + scroll_x) == s.size())
        {
            s.push_back(c);
        }
        else if(cursor_x + scroll_x < s.size())
        {
            s[cursor_x + scroll_x] = c;
        }
        else // cursor + scroll longer than string, pad with space
        {
            s.append(cursor_x + scroll_x - s.size(), ' ');
            s.back() = c;
        }
        e.updated = true;
        e.chars[cursor_x].bg = bg;
        e.chars[cursor_x].fg = fg;
        cursor_x += 1;
        if(cursor_x == COLS)
        {
            cursor_x -= 1;
            scroll_x += 1;
        }
    }
}
void screen::newLine()
{
    cursor_x = 0;
    scroll_x = 0;
    cursor_y += 1;
    if(cursor_y == ROWS)
    {
        cursor_y -= 1;
        std::rotate(row_elems.begin(), row_elems.begin() + 1, row_elems.end());
    }
    else
    {
        std::rotate(std::make_reverse_iterator(row_elems.begin() + cursor_y), std::make_reverse_iterator(row_elems.begin() + cursor_y + 1), row_elems.rend());
    }
    row_elems[cursor_y].value.clear();
    row_elems[cursor_y].updated = true;
}

void screen::draw()
{
    float y = 0.0f;
    for(std::size_t y_idx = 0; y_idx < ROWS; ++y_idx, y += charH)
    {
        float x = 0.0f;
        const auto& el = row_elems[y_idx];
        for(std::size_t x_idx = 0; x_idx < COLS; ++x_idx, x += charW)
        {
            const auto& c = el.chars[x_idx];
            C2D_DrawRectSolid(x, y, 0.0f, charW, charH, c.bg);
            if(c.have_fg)
            {
                C2D_DrawText(&c.text, C2D_WithColor, x, y, 0.25f, TEXT_SCALE, TEXT_SCALE, c.fg);
            }
            if(x_idx == cursor_x && y_idx == cursor_y && cursor_visible)
            {
                C2D_DrawRectSolid(x, y, 0.5f, 2.0f, charH, c.bg ^ 0xffffff);
            }
        }
    }
}
