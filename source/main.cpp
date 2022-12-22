#include <cstdint>
#include <cstring>
#include <cstdio>
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
#include <charconv>

#include <3ds.h>
#include <citro3d.h>
#include <citro2d.h>
#include "app.h"

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
    C2D_SpriteSheet sprites = C2D_SpriteSheetLoad("romfs:/gfx/sprites.t3x");
    int retval = 0;
    {
    auto app_ptr = std::make_unique<application>(mono_font, sprites);
    auto& app = *app_ptr;

    touchPosition touch;
    while(aptMainLoop() && !app.return_value())
    {
        hidScanInput();

        // Your code goes here
        const u32 kDown = hidKeysDown();
        const u32 kDownRepeat = hidKeysDownRepeat();
        const u32 kHeld = hidKeysHeld();
        const u32 kUp = hidKeysUp();

        app.read_output(10);

        if(kDown & KEY_START)
        {
            break;
        }
        if(kDown & KEY_A)
        {
            app.press_key("\n", !(kDownRepeat & ~kDown & KEY_A));
        }
        if(kDownRepeat & KEY_B)
        {
            app.press_key("\x08", !(kDownRepeat & ~kDown & KEY_B));
        }
        if(kDownRepeat & KEY_DUP)
        {
            app.press_key("\e[A", !(kDownRepeat & ~kDown & KEY_DUP));
        }
        else if(kDownRepeat & KEY_DDOWN)
        {
            app.press_key("\e[B", !(kDownRepeat & ~kDown & KEY_DDOWN));
        }
        else if(kDownRepeat & KEY_DLEFT)
        {
            app.press_key("\e[D", !(kDownRepeat & ~kDown & KEY_DLEFT));
        }
        else if(kDownRepeat & KEY_DRIGHT)
        {
            app.press_key("\e[C", !(kDownRepeat & ~kDown & KEY_DRIGHT));
        }

        if(kHeld & KEY_TOUCH)
        {
            hidTouchRead(&touch);
            if(kDown & KEY_TOUCH)
            {
                app.click_start_at(touch.px, touch.py);
            }
            else
            {
                app.click_move_to(touch.px, touch.py);
            }
        }
        else if(kUp & KEY_TOUCH)
        {
            app.click_release();
        }

        app.tick();

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

        C2D_TargetClear(top, C2D_Color32(0,0,0,255));
        C2D_TargetClear(bottom, C2D_Color32(0,0,0,255));

        C2D_SceneBegin(top);

        C2D_DrawRectSolid(400 - 1, 240 - 1, 0, 1, 1, -1);
        app.draw_top();

        C2D_SceneBegin(bottom);
        app.draw_bottom();

        C3D_FrameEnd(0);
    }

    if(auto r = app.return_value())
    {
        retval = *r;
    }
    }

    C2D_SpriteSheetFree(sprites);
    C2D_FontFree(mono_font);

    romfsExit();

    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return retval;
}
