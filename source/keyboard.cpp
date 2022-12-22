#include "keyboard.h"
#include <span>

const keyboard::pane& keyboard::get_active_pane() const
{
    return panes[shift_state & 3];
}

keyboard::keyboard()
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
