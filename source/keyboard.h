#pragma once

#include <vector>
#include <string>

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

    template<typename T>
    int do_press(int x, int y, T&& callback)
    {
        auto do_action = [this, &callback](char c)
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
                    break;
                case '\x02':
                    shift_state &= ~5;
                    shift_state ^= 2;
                    break;
                case '\x03': // backspace
                    callback('\x08');
                    break;
                case '\x04': // validate
                    return 1;
                case '\x05': // first
                    callback('\r');
                    break;
                case '\x06': // end
                    callback('\x00');
                    break;
                default:
                    break;
                }
            }
            else
            {
                callback(c);
                if((shift_state & 0x5) == 1)
                {
                    shift_state ^= 1;
                }
            }
            return 0;
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
    
    const pane& get_active_pane() const;
    keyboard();
};
