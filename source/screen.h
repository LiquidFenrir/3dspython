#pragma once

#include <string_view>
#include <charconv>
#include <string>
#include <array>

#include <3ds.h>
#include <citro3d.h>
#include <citro2d.h>

static inline constexpr std::array<u32, 256> buildColorTable()
{
    #define RGB8_TO_U32(a,b,c) C2D_Color32(a,b,c,255)
    std::array<u32, 256> colorTable;
    colorTable[0] = RGB8_TO_U32(0,0,0);
    colorTable[1] = RGB8_TO_U32(205,0,0);
    colorTable[2] = RGB8_TO_U32(0,205,0);
    colorTable[3] = RGB8_TO_U32(205,205,0);
    colorTable[4] = RGB8_TO_U32(0,0,238);
    colorTable[5] = RGB8_TO_U32(205,0,205);
    colorTable[6] = RGB8_TO_U32(0,205,205);
    colorTable[7] = RGB8_TO_U32(229,229,229);

    colorTable[0+8] = RGB8_TO_U32(127,127,127);
    colorTable[1+8] = RGB8_TO_U32(255,0,0);
    colorTable[2+8] = RGB8_TO_U32(0,255,0);
    colorTable[3+8] = RGB8_TO_U32(255,255,0);
    colorTable[4+8] = RGB8_TO_U32(92, 92, 255);
    colorTable[5+8] = RGB8_TO_U32(255,0,255);
    colorTable[6+8] = RGB8_TO_U32(0,255,255);
    colorTable[7+8] = RGB8_TO_U32(255,255,255);

    for(u8 r = 0; r <= 5; ++r)
        for(u8 g = 0; g <= 5; ++g)
            for(u8 b = 0; b <= 5; ++b)
                colorTable[16 + 36 * r + 6 * g + b] = RGB8_TO_U32(r == 0 ? 0 : (55 + 40 * r), g == 0 ? 0 : (55 + 40 * g), b == 0 ? 0 : (55 + 40 * b));

    for(u8 x = 8, i = 232; i != 0; x += 10, ++i)
        colorTable[i] = RGB8_TO_U32(x,x,x);

    #undef RGB8_TO_U32
    return colorTable;
}
struct screen {
    static constexpr inline std::size_t ROWS = 12;
    static constexpr inline std::size_t COLS = 30;
    std::size_t cursor_x{0}, cursor_y{0}, scroll_x{0};
    static inline constexpr float TEXT_SCALE = 0.5f + (1.0f / 4);
    unsigned frame_counter{0};
    bool cursor_visible{true};

    //set up the palette for color printing
    static inline constexpr std::array<u32, 256> FIXED_COLOR_TABLE = buildColorTable();

    struct character {
        C2D_Text text;
        u32 bg, fg;
        bool have_fg;
    };
    struct row {
        std::u32string value;
        std::array<character, COLS> chars;
        C2D_TextBuf buf;
        bool updated;
    };

    std::array<row, ROWS> row_elems;

    screen(C2D_Font fnt_arg);
    ~screen();

    void print(std::string_view str);
    void tick();
    void draw();

private:
    void printChar(char32_t c);
    void newLine();
    template<typename T>
    T readStr(std::string_view& from)
    {
        T out = 0;
        auto res = std::from_chars(from.begin(), from.end(), out, 10);
        const int diff = res.ptr - from.begin();
        if(diff)
            from.remove_prefix(diff);
        return out;
    }
    C2D_Font fnt;
    float charW, charH;
    u32 bg, fg;
    unsigned flags;
};
