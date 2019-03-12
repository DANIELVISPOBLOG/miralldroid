#include "screen.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string.h>

#include "icon.xpm"
#include "lock_util.h"
#include "log.h"
#include "tiny_xpm.h"

#define DISPLAY_MARGINS 96

#ifdef OVERRIDE_FONT_PATH
# define DEFAULT_FONT_PATH OVERRIDE_FONT_PATH
#else
# define DEFAULT_FONT_PATH PREFIX PREFIXED_FONT_PATH
#endif

SDL_bool sdl_init_and_configure(void) {
    if (SDL_Init(SDL_INIT_VIDEO)) {
        LOGC("Could not initialize SDL: %s", SDL_GetError());
        return SDL_FALSE;
    }

    atexit(SDL_Quit);

    // Use the best available scale quality
    if (!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2")) {
        LOGW("Could not enable bilinear filtering");
    }

#if SDL_VERSION_ATLEAST(2, 0, 5)
    // Handle a click to gain focus as any other click
    if (!SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1")) {
        LOGW("Could not enable mouse focus clickthrough");
    }
#endif

    // Do not disable the screensaver when scrcpy is running
    SDL_EnableScreenSaver();

    return SDL_TRUE;
}

// get the window size in a struct size
static struct size get_native_window_size(SDL_Window *window) {
    int width;
    int height;
    SDL_GetWindowSize(window, &width, &height);

    struct size size;
    size.width = width;
    size.height = height;
    return size;
}

// get the windowed window size
static struct size get_window_size(const struct screen *screen) {
    if (screen->fullscreen) {
        return screen->windowed_window_size;
    }
    return get_native_window_size(screen->window);
}

// set the window size to be applied when fullscreen is disabled
static void set_window_size(struct screen *screen, struct size new_size) {
    // setting the window size during fullscreen is implementation defined,
    // so apply the resize only after fullscreen is disabled
    if (screen->fullscreen) {
        // SDL_SetWindowSize will be called when fullscreen will be disabled
        screen->windowed_window_size = new_size;
    } else {
        SDL_SetWindowSize(screen->window, new_size.width, new_size.height);
    }
}

// get the preferred display bounds (i.e. the screen bounds with some margins)
static SDL_bool get_preferred_display_bounds(struct size *bounds) {
    SDL_Rect rect;
#if SDL_VERSION_ATLEAST(2, 0, 5)
# define GET_DISPLAY_BOUNDS(i, r) SDL_GetDisplayUsableBounds((i), (r))
#else
# define GET_DISPLAY_BOUNDS(i, r) SDL_GetDisplayBounds((i), (r))
#endif
    if (GET_DISPLAY_BOUNDS(0, &rect)) {
        LOGW("Could not get display usable bounds: %s", SDL_GetError());
        return SDL_FALSE;
    }

    bounds->width = MAX(0, rect.w - DISPLAY_MARGINS);
    bounds->height = MAX(0, rect.h - DISPLAY_MARGINS);
    return SDL_TRUE;
}

// return the optimal size of the window, with the following constraints:
//  - it attempts to keep at least one dimension of the current_size (i.e. it crops the black borders)
//  - it keeps the aspect ratio
//  - it scales down to make it fit in the display_size
static struct size get_optimal_size(struct size current_size, struct size frame_size) {
    if (frame_size.width == 0 || frame_size.height == 0) {
        // avoid division by 0
        return current_size;
    }

    struct size display_size;
    // 32 bits because we need to multiply two 16 bits values
    Uint32 w;
    Uint32 h;

    if (!get_preferred_display_bounds(&display_size)) {
        // cannot get display bounds, do not constraint the size
        w = current_size.width;
        h = current_size.height;
    } else {
        w = MIN(current_size.width, display_size.width);
        h = MIN(current_size.height, display_size.height);
    }

    SDL_bool keep_width = frame_size.width * h > frame_size.height * w;
    if (keep_width) {
        // remove black borders on top and bottom
        h = frame_size.height * w / frame_size.width;
    } else {
        // remove black borders on left and right (or none at all if it already fits)
        w = frame_size.width * h / frame_size.height;
    }

    // w and h must fit into 16 bits
    SDL_assert_release(w < 0x10000 && h < 0x10000);
    return (struct size) {w, h};
}

// same as get_optimal_size(), but read the current size from the window
static inline struct size get_optimal_window_size(const struct screen *screen, struct size frame_size) {
    struct size current_size = get_window_size(screen);
    return get_optimal_size(current_size, frame_size);
}

// initially, there is no current size, so use the frame size as current size
static inline struct size get_initial_optimal_size(struct size frame_size) {
    return get_optimal_size(frame_size, frame_size);
}

int SDL_SetRenderDrawColor2(SDL_Renderer* renderer, SDL_Color color) {
    return SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

void toolbar_build_rects(struct screen *screen, int toolbar_index,  int init_x, int init_y, int mousediff_x, int mousediff_y){
    //Calculate Container frame rect
    SDL_Rect framerect;
    framerect.x = init_x;
    framerect.y = init_y;
    framerect.w= screen->frame_size.width - TOOLBAR_SPACER_WIDTH * 2;
    framerect.h= TOOLBAR_HEIGHT;
    screen->toolbar[toolbar_index].rect=framerect;
    
    screen->toolbar[toolbar_index].drag_mousediff.x=mousediff_x;
    screen->toolbar[toolbar_index].drag_mousediff.y=mousediff_y;

    //Calculate buttons
    const int nbuttons=sizeof(screen->toolbar[toolbar_index].buttons) / sizeof(struct button);
    for (int j=0;j<nbuttons;j++){
        SDL_Rect buttonrect;
        buttonrect.x=(j==0)
                      ? screen->toolbar[toolbar_index].rect.x+TOOLBAR_BUTTON_SPACER
                      : screen->toolbar[toolbar_index].buttons[j-1].rect.x + screen->toolbar[toolbar_index].buttons[j-1].rect.w + TOOLBAR_BUTTON_SPACER;
        buttonrect.y=(j==0)
                      ? screen->toolbar[toolbar_index].rect.y+TOOLBAR_BUTTON_SPACER
                      : screen->toolbar[toolbar_index].buttons[j-1].rect.y;
        buttonrect.w=(j==0)
                      ? (screen->toolbar[toolbar_index].rect.w - TOOLBAR_BUTTON_SPACER * (nbuttons+1)) / nbuttons
                      : screen->toolbar[toolbar_index].buttons[j-1].rect.w;
        buttonrect.h=(j==0)
                      ? screen->toolbar[toolbar_index].rect.h - TOOLBAR_BUTTON_SPACER * 2
                      : screen->toolbar[toolbar_index].buttons[j-1].rect.h;
        screen->toolbar[toolbar_index].buttons[j].rect=buttonrect;
    }
}

SDL_bool toolbar_button_render_icon(struct screen *screen, int toolbar_index, int button_index) {
    SDL_Surface* glyph_surface = TTF_RenderGlyph_Blended(screen->toolbar_font, screen->toolbar[toolbar_index].buttons[button_index].unicode_glyph, screen->toolbar[toolbar_index].icon_color);
    if (!glyph_surface) return SDL_FALSE;
    SDL_Texture* glyph_texture = SDL_CreateTextureFromSurface(screen->renderer, glyph_surface);
    if (!glyph_texture) return SDL_FALSE;

    SDL_FreeSurface(glyph_surface);
    SDL_Rect glyph_rect;
    glyph_rect.x = screen->toolbar[toolbar_index].buttons[button_index].rect.x + (screen->toolbar[toolbar_index].buttons[button_index].rect.w/2) - ((TOOLBAR_HEIGHT - TOOLBAR_BUTTON_SPACER*2 - (TOOLBAR_HEIGHT - TOOLBAR_BUTTON_ICON_WIDTH))/2);
    glyph_rect.y = screen->toolbar[toolbar_index].buttons[button_index].rect.y + (screen->toolbar[toolbar_index].buttons[button_index].rect.h/2) - ((TOOLBAR_HEIGHT - TOOLBAR_BUTTON_SPACER*2 - (TOOLBAR_HEIGHT - TOOLBAR_BUTTON_ICON_HEIGHT))/2);
    glyph_rect.w = TOOLBAR_HEIGHT - TOOLBAR_BUTTON_SPACER*2 - (TOOLBAR_HEIGHT-TOOLBAR_BUTTON_ICON_WIDTH);
    glyph_rect.h = TOOLBAR_HEIGHT - TOOLBAR_BUTTON_SPACER*2 - (TOOLBAR_HEIGHT-TOOLBAR_BUTTON_ICON_HEIGHT);
    if (SDL_RenderCopy(screen->renderer, glyph_texture, NULL, &glyph_rect)) return SDL_FALSE;
    SDL_DestroyTexture(glyph_texture);
    return SDL_TRUE;
}

void toolbar_render(struct screen *screen) {
    if (screen->toolbar_shown) {

        int toolbarqty = sizeof(screen->toolbar) / sizeof(struct toolbar);

        for (int i=0;i<toolbarqty;i++) {
            int toolbar_index = screen->toolbar_zindex_sort_down_to_top[i];
            if (screen->toolbar[toolbar_index].shown) {
                SDL_Color frame_color=screen->toolbar[toolbar_index].background_default_color;
                if (screen->toolbar[toolbar_index].drag_pos.x != -9999 && screen->toolbar[toolbar_index].drag)
                      frame_color=screen->toolbar[toolbar_index].background_highlight_color;
                if (SDL_SetRenderDrawColor2(screen->renderer, frame_color)){
                    LOGW("Cannot render Toolbar[%d] frame background color: %s",toolbar_index,SDL_GetError());
                    return;
                }
                if (SDL_RenderFillRect(screen->renderer, &screen->toolbar[toolbar_index].rect)) {
                    LOGW("Cannot render Toolbar[%d] frame: %s",toolbar_index,SDL_GetError());
                    return;
                }
  
                int buttonqty = sizeof(screen->toolbar[toolbar_index].buttons) / sizeof(struct button);

                for (int j=0;j<buttonqty;j++){
                    if (screen->toolbar[toolbar_index].buttons[j].mouseover){
                        if (SDL_SetRenderDrawColor2(screen->renderer, screen->toolbar[toolbar_index].background_hover_color)){
                            LOGW("Cannot render Toolbar[%d] button 1 hover color: %s",toolbar_index,SDL_GetError());
                            return;
                        }
                    } else {
                        if (SDL_SetRenderDrawColor2(screen->renderer, screen->toolbar[toolbar_index].background_default_color)) {
                            LOGW("Cannot render Toolbar[%d] button 1 background color: %s",toolbar_index,SDL_GetError());
                            return;
                        }
                    }
                    if (SDL_RenderFillRect(screen->renderer, &screen->toolbar[toolbar_index].buttons[j].rect)) {
                        LOGW("Cannot render Toolbar[%d] button [%d]: %s",toolbar_index,j,SDL_GetError());
                        return;
                    }
                    if (!toolbar_button_render_icon(screen,toolbar_index,j)){
                        LOGW("Cannot render Toolbar[%d] button [%d] icon: %s",toolbar_index,j,SDL_GetError());
                        return;
                    }
                }
            }
        }
    }
}

void set_background_color(struct screen *screen) {
    Uint8 colorR,colorG,colorB,colorA; 
    colorR=255;
    colorG=255;
    colorB=255;
    colorA=100;
    SDL_SetRenderDrawColor(screen->renderer, colorR, colorG, colorB, colorA);
}

void screen_init(struct screen *screen) {
    *screen = (struct screen) SCREEN_INITIALIZER;
}

static inline SDL_Texture *create_texture(struct screen *screen, struct size frame_size) {
    return SDL_CreateTexture(screen->renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING,
                             frame_size.width, frame_size.height);
}

SDL_bool screen_init_rendering(struct screen *screen, const char *device_name, struct size frame_size) {
    //Initialize TTF Font
    if (TTF_Init()==-1){
        LOGW("Could not initialize TTF: %s", SDL_GetError());
        return SDL_FALSE;
    }
    
    const char *font_path = getenv("MIRALLDROID_FONT_PATH");
    if (!font_path) {
        font_path = DEFAULT_FONT_PATH;
    }
    screen->toolbar_font = TTF_OpenFont(font_path, TOOLBAR_BUTTON_ICON_HEIGHT);
    if (!screen->toolbar_font) {
        LOGW("Could not open font: %s", SDL_GetError());
        return SDL_FALSE;
    }

    screen->frame_size = frame_size;

    struct size window_size = get_initial_optimal_size(frame_size);
    Uint32 window_flags = SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE;
#ifdef HIDPI_SUPPORT
    window_flags |= SDL_WINDOW_ALLOW_HIGHDPI;
#endif
    screen->window = SDL_CreateWindow(device_name, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                      window_size.width, window_size.height, window_flags);
    if (!screen->window) {
        LOGC("Could not create window: %s", SDL_GetError());
        return SDL_FALSE;
    }

    screen->renderer = SDL_CreateRenderer(screen->window, -1, SDL_RENDERER_ACCELERATED);
    if (!screen->renderer) {
        LOGC("Could not create renderer: %s", SDL_GetError());
        screen_destroy(screen);
        return SDL_FALSE;
    }

    if (SDL_SetRenderDrawBlendMode(screen->renderer, SDL_BLENDMODE_BLEND)) {
        LOGE("Could not set renderer draw blend mode: %s", SDL_GetError());
        screen_destroy(screen);
        return SDL_FALSE;
    }

    if (SDL_RenderSetLogicalSize(screen->renderer, frame_size.width, frame_size.height)) {
        LOGE("Could not set renderer logical size: %s", SDL_GetError());
        screen_destroy(screen);
        return SDL_FALSE;
    }

    SDL_Surface *icon = read_xpm(icon_xpm);
    if (!icon) {
        LOGE("Could not load icon: %s", SDL_GetError());
        screen_destroy(screen);
        return SDL_FALSE;
    }
    SDL_SetWindowIcon(screen->window, icon);
    SDL_FreeSurface(icon);

    LOGI("Initial texture: %" PRIu16 "x%" PRIu16, frame_size.width, frame_size.height);
    screen->texture = create_texture(screen, frame_size);
    if (!screen->texture) {
        LOGC("Could not create texture: %s", SDL_GetError());
        screen_destroy(screen);
        return SDL_FALSE;
    }

    int toolbarqty = sizeof(screen->toolbar) / sizeof(struct toolbar);
    for (int i=0; i<toolbarqty; i++) {
        //TODO refactoring
        if (i==0) toolbar_build_rects(screen, i, TOOLBAR_SPACER_WIDTH, TOOLBAR_SPACER_HEIGHT, 0, 0);
        if (i==1) toolbar_build_rects(screen, i, TOOLBAR_SPACER_WIDTH, screen->frame_size.height - TOOLBAR_SPACER_HEIGHT - TOOLBAR_HEIGHT, 0, 0);
    }
    
    return SDL_TRUE;
}

void screen_show_window(struct screen *screen) {
    SDL_ShowWindow(screen->window);
}

void screen_destroy(struct screen *screen) {
    if (screen->toolbar_font) {
        TTF_CloseFont(screen->toolbar_font);
        TTF_Quit();
    }
    if (screen->texture) {
        SDL_DestroyTexture(screen->texture);
    }
    if (screen->renderer) {
        SDL_DestroyRenderer(screen->renderer);
    }
    if (screen->window) {
        SDL_DestroyWindow(screen->window);
    }
}

// recreate the texture and resize the window if the frame size has changed
static SDL_bool prepare_for_frame(struct screen *screen, struct size new_frame_size) {
    if (screen->frame_size.width != new_frame_size.width || screen->frame_size.height != new_frame_size.height) {
        if (SDL_RenderSetLogicalSize(screen->renderer, new_frame_size.width, new_frame_size.height)) {
            LOGE("Could not set renderer logical size: %s", SDL_GetError());
            return SDL_FALSE;
        }

        // frame dimension changed, destroy texture
        SDL_DestroyTexture(screen->texture);

        struct size current_size = get_window_size(screen);
        struct size target_size = {
            (Uint32) current_size.width * new_frame_size.width / screen->frame_size.width,
            (Uint32) current_size.height * new_frame_size.height / screen->frame_size.height,
        };
        target_size = get_optimal_size(target_size, new_frame_size);
        set_window_size(screen, target_size);

        screen->frame_size = new_frame_size;

        LOGD("New texture: %" PRIu16 "x%" PRIu16,
                     screen->frame_size.width, screen->frame_size.height);
        screen->texture = create_texture(screen, new_frame_size);
        if (!screen->texture) {
            LOGC("Could not create texture: %s", SDL_GetError());
            return SDL_FALSE;
        }

        int toolbarqty = sizeof(screen->toolbar) / sizeof(struct toolbar);
        for (int i=0; i<toolbarqty; i++) {
            //TODO refactoring
            if (i==0) toolbar_build_rects(screen, i, TOOLBAR_SPACER_WIDTH, TOOLBAR_SPACER_HEIGHT, 0, 0);
            if (i==1) toolbar_build_rects(screen, i, TOOLBAR_SPACER_WIDTH, screen->frame_size.height - TOOLBAR_SPACER_HEIGHT - TOOLBAR_HEIGHT, 0, 0);
        }
    }

    return SDL_TRUE;
}

// write the frame into the texture
static void update_texture(struct screen *screen, const AVFrame *frame) {
    SDL_UpdateYUVTexture(screen->texture, NULL,
            frame->data[0], frame->linesize[0],
            frame->data[1], frame->linesize[1],
            frame->data[2], frame->linesize[2]);
}

SDL_bool screen_update_frame(struct screen *screen, struct frames *frames) {
    mutex_lock(frames->mutex);
    const AVFrame *frame = frames_consume_rendered_frame(frames);
    struct size new_frame_size = {frame->width, frame->height};
    if (!prepare_for_frame(screen, new_frame_size)) {
        mutex_unlock(frames->mutex);
        return SDL_FALSE;
    }
    update_texture(screen, frame);
    mutex_unlock(frames->mutex);

    return SDL_TRUE;
}

void screen_render(struct screen *screen) {
    set_background_color(screen);
    SDL_RenderClear(screen->renderer);
    SDL_RenderCopy(screen->renderer, screen->texture, NULL, NULL);
    toolbar_render(screen);
    SDL_RenderPresent(screen->renderer);
}

void screen_switch_fullscreen(struct screen *screen) {
    if (!screen->fullscreen) {
        // going to fullscreen, store the current windowed window size
        screen->windowed_window_size = get_native_window_size(screen->window);
    }
    Uint32 new_mode = screen->fullscreen ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP;
    if (SDL_SetWindowFullscreen(screen->window, new_mode)) {
        LOGW("Could not switch fullscreen mode: %s", SDL_GetError());
        return;
    }

    screen->fullscreen = !screen->fullscreen;
    if (!screen->fullscreen) {
        // fullscreen disabled, restore expected windowed window size
        SDL_SetWindowSize(screen->window, screen->windowed_window_size.width, screen->windowed_window_size.height);
    }

    LOGD("Switched to %s mode", screen->fullscreen ? "fullscreen" : "windowed");
}

void toolbar_toggle(struct screen *screen) {
    screen->toolbar_shown = (screen->toolbar_shown) ? SDL_FALSE : SDL_TRUE;
    LOGD("Onscreen menus are %s", screen->toolbar_shown ? "shown" : "hidden");
}

void screen_resize_to_fit(struct screen *screen) {
    if (!screen->fullscreen) {
        struct size optimal_size = get_optimal_window_size(screen, screen->frame_size);
        SDL_SetWindowSize(screen->window, optimal_size.width, optimal_size.height);
        LOGD("Resized to optimal size");
    }
}

void screen_resize_to_pixel_perfect(struct screen *screen) {
    if (!screen->fullscreen) {
        SDL_SetWindowSize(screen->window, screen->frame_size.width, screen->frame_size.height);
        LOGD("Resized to pixel-perfect");
    }
}
