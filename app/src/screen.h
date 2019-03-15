#ifndef SCREEN_H
#define SCREEN_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <libavformat/avformat.h>

#include "common.h"
#include "frames.h"

enum TOOLBAR_BUTTON {
    POWER,
    VOLUME_DOWN,
    VOLUME_UP,
    SWITCH,
    HOME,
    BACK
};

struct button {
    enum TOOLBAR_BUTTON type;
    SDL_Rect rect;
    SDL_bool mouseover;
    Uint16 unicode_glyph;
};

struct toolbar {
    SDL_bool shown;
    SDL_Rect rect;
    SDL_bool drag;
    SDL_Point drag_pos;
    SDL_Point drag_mousediff;
    SDL_Color background_default_color;
    SDL_Color background_hover_color;
    SDL_Color background_highlight_color;
    SDL_Color icon_color;
    struct button buttons[3];
};

struct screen {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    struct size frame_size;
    //used only in fullscreen mode to know the windowed window size
    struct size windowed_window_size;
    SDL_bool has_frame;
    SDL_bool fullscreen;
    SDL_bool toolbar_shown;
    TTF_Font* toolbar_font;
    int toolbar_zindex_sort_down_to_top[2];
    struct toolbar toolbar[2];
};

#define TOOLBAR_SPACER_WIDTH 200
#define TOOLBAR_SPACER_HEIGHT 50
#define TOOLBAR_HEIGHT 100
#define TOOLBAR_BUTTON_SPACER 10
#define TOOLBAR_BUTTON_ICON_HEIGHT 70
#define TOOLBAR_BUTTON_ICON_WIDTH 70

#define SCREEN_INITIALIZER {                                  \
    .window = NULL,                                           \
    .renderer = NULL,                                         \
    .texture = NULL,                                          \
    .frame_size = {                                           \
        .width = 0,                                           \
        .height = 0,                                          \
    },                                                        \
    .windowed_window_size = {                                 \
        .width = 0,                                           \
        .height = 0,                                          \
    },                                                        \
    .has_frame = SDL_FALSE,                                   \
    .fullscreen = SDL_FALSE,                                  \
    .toolbar_shown = SDL_TRUE,                                \
    .toolbar_font = NULL,                                     \
    .toolbar_zindex_sort_down_to_top={0,1},                   \
        .toolbar[0] = {                                       \
        .shown = SDL_TRUE,                                    \
        .rect = {0,0,0,0},                                    \
        .drag = SDL_FALSE,                                    \
        .drag_pos = {-9999,-9999},                            \
        .drag_mousediff = {0,0},                              \
        .background_default_color = {0,0,0,100},              \
        .background_hover_color = {255,255,255,100},          \
        .background_highlight_color = {255,255,255,100},      \
        .icon_color = {255,255,255},                          \
        .buttons[0]= {                                        \
            .type = POWER,                                    \
            .rect = {0,0,0,0},                                \
            .mouseover = SDL_FALSE,                           \
            .unicode_glyph = u'\ue801'                        \
        },                                                    \
        .buttons[1]= {                                        \
            .type = VOLUME_DOWN,                              \
            .rect = {0,0,0,0},                                \
            .mouseover = SDL_FALSE,                           \
            .unicode_glyph = u'\ue804'                        \
        },                                                    \
        .buttons[2]= {                                        \
            .type = VOLUME_UP,                                \
            .rect = {0,0,0,0},                                \
            .mouseover = SDL_FALSE,                           \
            .unicode_glyph = u'\ue805'                        \
        }                                                     \
    },                                                        \
    .toolbar[1] = {                                           \
        .shown = SDL_TRUE,                                    \
        .rect = {0,0,0,0},                                    \
        .drag = SDL_FALSE,                                    \
        .drag_pos = {-9999,-9999},                            \
        .drag_mousediff = {0,0},                              \
        .background_default_color = {0,0,0,100},              \
        .background_hover_color = {255,255,255,100},          \
        .background_highlight_color = {255,255,255,100},      \
        .icon_color = {255,255,255},                          \
        .buttons[0]= {                                        \
            .type = SWITCH,                                   \
            .rect = {0,0,0,0},                                \
            .mouseover = SDL_FALSE,                           \
            .unicode_glyph = u'\ue803'                        \
        },                                                    \
        .buttons[1]= {                                        \
            .type = HOME,                                     \
            .rect = {0,0,0,0},                                \
            .mouseover = SDL_FALSE,                           \
            .unicode_glyph = u'\ue800'                        \
        },                                                    \
        .buttons[2]= {                                        \
            .type = BACK,                                     \
            .rect = {0,0,0,0},                                \
            .mouseover = SDL_FALSE,                           \
            .unicode_glyph = u'\ue802'                        \
        }                                                     \
    }                                                         \
}

// init SDL and set appropriate hints
SDL_bool sdl_init_and_configure(void);

// initialize default values
void screen_init(struct screen *screen);

// initialize screen, create window, renderer and texture (window is hidden)
SDL_bool screen_init_rendering(struct screen *screen,
                               const char *device_name,
                               struct size frame_size,
                               SDL_bool always_on_top);

// show the window
void screen_show_window(struct screen *screen);

// destroy window, renderer and texture (if any)
void screen_destroy(struct screen *screen);

// resize if necessary and write the rendered frame into the texture
SDL_bool screen_update_frame(struct screen *screen, struct frames *frames);

// render the texture to the renderer
void screen_render(struct screen *screen);

// switch the fullscreen mode
void screen_switch_fullscreen(struct screen *screen);

// toggle onscreen_menus
void toolbar_toggle(struct screen *screen);

// resize window to optimal size (remove black borders)
void screen_resize_to_fit(struct screen *screen);

// resize window to 1:1 (pixel-perfect)
void screen_resize_to_pixel_perfect(struct screen *screen);

// calculate toolbar and buttons draw SDL_Rects
void toolbar_build_rects(struct screen *screen, int toolbar_index,  int init_x, int init_y, int mousediff_x, int mousediff_y);

#endif
