#include "ui.h"
#include <SDL2/SDL.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>

static SDL_Window *g_win = NULL;
static SDL_Renderer *g_rend = NULL;
static int g_touch_x = 0;
static int g_touch_y = 0;
static bool g_touch_pressed = false;
static bool g_touch_just_released = false;

static int g_active_field = -1;
static uint32_t g_cursor_blink = 0;
static char g_text_in[256] = {0};

static char g_log_lines[8][96];
static int g_log_count = 0;

static char g_set_server[128] = "server.duinocoin.com";
static char g_set_port[8] = "12181";
static char g_set_user[64] = "vita_miner";
static char g_set_worker[64] = "";
static char g_set_key[64] = "";
static char g_set_donation[64] = "";
static bool g_set_auto_start = false;
static bool g_settings_dirty = false;

static const uint8_t font[95][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // space
    {0x00,0x00,0x38,0x00,0x38,0x00,0x00,0x00}, // !
    {0x00,0x36,0x36,0x00,0x00,0x00,0x00,0x00}, // "
    {0x00,0x36,0x36,0x7e,0x36,0x7e,0x36,0x00}, // #
    {0x00,0x0c,0x3e,0x03,0x1e,0x30,0x1e,0x00}, // $
    {0x00,0x62,0x66,0x0c,0x18,0x30,0x66,0x00}, // %
    {0x00,0x38,0x6c,0x38,0x38,0x6c,0x38,0x00}, // &
    {0x00,0x06,0x06,0x03,0x00,0x00,0x00,0x00}, // '
    {0x00,0x1c,0x30,0x60,0x60,0x30,0x1c,0x00}, // (
    {0x00,0x07,0x06,0x06,0x06,0x06,0x0f,0x00}, // )
    {0x00,0x00,0x36,0x36,0x7e,0x36,0x36,0x00}, // *
    {0x00,0x00,0x0c,0x0c,0x3e,0x0c,0x0c,0x00}, // +
    {0x00,0x00,0x00,0x00,0x00,0x06,0x06,0x00}, // ,
    {0x00,0x00,0x00,0x00,0x3e,0x00,0x00,0x00}, // -
    {0x00,0x00,0x00,0x00,0x00,0x30,0x30,0x00}, // .
    {0x00,0x00,0x06,0x0c,0x18,0x30,0x60,0x00}, // /
    {0x3e,0x63,0x73,0x7b,0x7b,0x63,0x3e,0x00}, // 0
    {0x0c,0x0e,0x0d,0x3e,0x0c,0x0c,0x3e,0x00}, // 1
    {0x3c,0x66,0x06,0x0c,0x18,0x30,0x7e,0x00}, // 2
    {0x3e,0x66,0x06,0x1c,0x06,0x66,0x3e,0x00}, // 3
    {0x08,0x18,0x28,0x68,0xfe,0x08,0x08,0x00}, // 4
    {0x3e,0x60,0x3c,0x06,0x06,0x66,0x3e,0x00}, // 5
    {0x3c,0x60,0x3c,0x66,0x66,0x66,0x3e,0x00}, // 6
    {0x3e,0x66,0x06,0x0c,0x18,0x18,0x18,0x00}, // 7
    {0x3e,0x66,0x3e,0x66,0x66,0x66,0x3e,0x00}, // 8
    {0x3e,0x66,0x3e,0x06,0x0c,0x18,0x30,0x00}, // 9
    {0x00,0x36,0x36,0x00,0x36,0x36,0x00,0x00}, // :
    {0x00,0x36,0x36,0x00,0x36,0x36,0x00,0x00}, // ; (same as :)
    {0x00,0x06,0x0c,0x18,0x30,0x18,0x0c,0x00}, // <
    {0x00,0x00,0x7e,0x00,0x7e,0x00,0x00,0x00}, // =
    {0x00,0x60,0x30,0x18,0x0c,0x18,0x30,0x00}, // >
    {0x00,0x3e,0x66,0x02,0x0c,0x18,0x00,0x00}, // ?
    {0x00,0x3e,0x66,0x7e,0x66,0x66,0x3e,0x00}, // @
    {0x3e,0x66,0x66,0x7e,0x66,0x66,0x66,0x00}, // A
    {0x7c,0x66,0x66,0x7c,0x66,0x66,0x7c,0x00}, // B
    {0x3e,0x66,0x60,0x60,0x60,0x66,0x3e,0x00}, // C
    {0x7c,0x66,0x66,0x66,0x66,0x66,0x7c,0x00}, // D
    {0x7e,0x60,0x60,0x7c,0x60,0x60,0x7e,0x00}, // E
    {0x7e,0x60,0x60,0x7c,0x60,0x60,0x60,0x00}, // F
    {0x3e,0x66,0x60,0x6e,0x66,0x66,0x3e,0x00}, // G
    {0x66,0x66,0x66,0x7e,0x66,0x66,0x66,0x00}, // H
    {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, // I
    {0x0c,0x0c,0x0c,0x0c,0x0c,0x6c,0x38,0x00}, // J
    {0x66,0x66,0x66,0x3c,0x18,0x66,0x66,0x00}, // K
    {0x60,0x60,0x60,0x60,0x60,0x60,0x7e,0x00}, // L
    {0x63,0x77,0x7f,0x7f,0x6b,0x63,0x63,0x00}, // M
    {0x63,0x73,0x7b,0x6f,0x67,0x63,0x63,0x00}, // N
    {0x3e,0x66,0x66,0x66,0x66,0x66,0x3e,0x00}, // O
    {0x7c,0x66,0x66,0x7c,0x60,0x60,0x60,0x00}, // P
    {0x3e,0x66,0x66,0x66,0x6e,0x66,0x3e,0x00}, // Q (close enough)
    {0x7c,0x66,0x66,0x7c,0x78,0x6c,0x66,0x00}, // R
    {0x3e,0x66,0x60,0x3e,0x06,0x66,0x3e,0x00}, // S
    {0x7e,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, // T
    {0x66,0x66,0x66,0x66,0x66,0x66,0x3e,0x00}, // U
    {0x66,0x66,0x66,0x66,0x66,0x3c,0x18,0x00}, // V
    {0x63,0x63,0x63,0x6b,0x7f,0x77,0x63,0x00}, // W
    {0x66,0x66,0x3c,0x18,0x3c,0x66,0x66,0x00}, // X
    {0x66,0x66,0x3c,0x18,0x18,0x18,0x18,0x00}, // Y
    {0x7e,0x06,0x0c,0x18,0x30,0x60,0x7e,0x00}, // Z
    {0x3e,0x66,0x66,0x66,0x66,0x66,0x3e,0x00}, // [ (close)
    {0x00,0x3e,0x00,0x3e,0x00,0x00,0x00,0x00}, // \ (close)
    {0x00,0x00,0x00,0x00,0x7e,0x00,0x00,0x00}, // ] (close)
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // ^
    {0x00,0x00,0x00,0x00,0x00,0x00,0x3e,0x00}, // _
    {0x00,0x00,0x03,0x06,0x0c,0x18,0x30,0x00}, // `
    {0x00,0x00,0x3c,0x66,0x7e,0x66,0x66,0x00}, // a
    {0x00,0x60,0x60,0x7c,0x66,0x66,0x7c,0x00}, // b
    {0x00,0x00,0x3c,0x66,0x60,0x66,0x3c,0x00}, // c
    {0x00,0x06,0x06,0x7c,0x66,0x66,0x7c,0x00}, // d
    {0x00,0x00,0x3c,0x66,0x7e,0x60,0x3c,0x00}, // e
    {0x00,0x0c,0x18,0x7e,0x18,0x18,0x18,0x00}, // f
    {0x00,0x00,0x3c,0x66,0x7e,0x06,0x7c,0x00}, // g
    {0x00,0x60,0x60,0x7c,0x66,0x66,0x66,0x00}, // h
    {0x00,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, // i
    {0x00,0x0c,0x0c,0x0c,0x0c,0x6c,0x38,0x00}, // j
    {0x00,0x60,0x60,0x66,0x6e,0x76,0x66,0x00}, // k
    {0x00,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, // l
    {0x00,0x00,0x7c,0x66,0x6e,0x76,0x66,0x00}, // m
    {0x00,0x00,0x7c,0x66,0x66,0x66,0x66,0x00}, // n
    {0x00,0x00,0x3c,0x66,0x66,0x66,0x3c,0x00}, // o
    {0x00,0x00,0x7c,0x66,0x66,0x7c,0x60,0x60}, // p
    {0x00,0x60,0x60,0x7c,0x66,0x66,0x7c,0x00}, // q
    {0x00,0x00,0x7c,0x66,0x60,0x60,0x60,0x00}, // r
    {0x00,0x00,0x3c,0x60,0x3c,0x06,0x7c,0x00}, // s
    {0x00,0x18,0x18,0x7e,0x18,0x18,0x00,0x00}, // t
    {0x00,0x00,0x66,0x66,0x66,0x66,0x3e,0x00}, // u
    {0x00,0x00,0x66,0x66,0x66,0x3c,0x18,0x00}, // v
    {0x00,0x00,0x63,0x6b,0x7f,0x77,0x63,0x00}, // w
    {0x00,0x00,0x66,0x3c,0x18,0x3c,0x66,0x00}, // x
    {0x00,0x00,0x66,0x66,0x3e,0x06,0x7c,0x00}, // y
    {0x00,0x00,0x7e,0x06,0x0c,0x18,0x7e,0x00}, // z
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // { (close)
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // | (close)
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // } (close)
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}  // ~
};

static int char_index(char c) {
    if (c == ' ') return 0;
    if (c == '!') return 1;
    if (c == '"') return 2;
    if (c == '#') return 3;
    if (c == '$') return 4;
    if (c == '%') return 5;
    if (c == '&') return 6;
    if (c == '\'') return 7;
    if (c == '(') return 8;
    if (c == ')') return 9;
    if (c == '*') return 10;
    if (c == '+') return 11;
    if (c == ',') return 12;
    if (c == '-') return 13;
    if (c == '.') return 14;
    if (c == '/') return 15;
    if (c >= '0' && c <= '9') return 16 + (c - '0');
    if (c == ':') return 26;
    if (c == ';') return 27;
    if (c == '<') return 28;
    if (c == '=') return 29;
    if (c == '>') return 30;
    if (c == '?') return 31;
    if (c == '@') return 32;
    if (c >= 'A' && c <= 'Z') return 33 + (c - 'A');
    if (c == '[') return 59;
    if (c == '\\') return 60;
    if (c == ']') return 61;
    if (c == '^') return 62;
    if (c == '_') return 63;
    if (c == '`') return 64;
    if (c >= 'a' && c <= 'z') return 65 + (c - 'a');
    return 0;
}

static void draw_char(SDL_Renderer *r, int x, int y, char c, uint8_t rr, uint8_t gg, uint8_t bb) {
    int idx = char_index(c);
    if (idx >= 95) idx = 0;
    SDL_SetRenderDrawColor(r, rr, gg, bb, 255);
    for (int row = 0; row < 8; row++) {
        uint8_t bits = font[idx][row];
        for (int col = 0; col < 6; col++) {
            if (bits & (0x20 >> col)) {
                SDL_Rect rect = { x + col * 3, y + row * 3, 3, 3 };
                SDL_RenderFillRect(r, &rect);
            }
        }
    }
}

static void draw_text(SDL_Renderer *r, int x, int y, const char *s, uint8_t rr, uint8_t gg, uint8_t bb) {
    int cx = x;
    while (*s) {
        draw_char(r, cx, y, *s, rr, gg, bb);
        cx += 21;
        s++;
    }
}

static void draw_text_small(SDL_Renderer *r, int x, int y, const char *s, uint8_t rr, uint8_t gg, uint8_t bb) {
    int cx = x;
    while (*s) {
        draw_char(r, cx, y, *s, rr, gg, bb);
        cx += 18;
        s++;
    }
}

static void fill_rect(SDL_Renderer *r, int x, int y, int w, int h, uint8_t rr, uint8_t gg, uint8_t bb, uint8_t aa) {
    SDL_SetRenderDrawColor(r, rr, gg, bb, aa);
    SDL_Rect rect = { x, y, w, h };
    SDL_RenderFillRect(r, &rect);
}

static void draw_rect_outline(SDL_Renderer *r, int x, int y, int w, int h, uint8_t rr, uint8_t gg, uint8_t bb) {
    SDL_SetRenderDrawColor(r, rr, gg, bb, 255);
    SDL_Rect r1 = { x, y, w, 2 };
    SDL_Rect r2 = { x, y + h - 2, w, 2 };
    SDL_Rect r3 = { x, y, 2, h };
    SDL_Rect r4 = { x + w - 2, y, 2, h };
    SDL_RenderFillRect(r, &r1);
    SDL_RenderFillRect(r, &r2);
    SDL_RenderFillRect(r, &r3);
    SDL_RenderFillRect(r, &r4);
}

static int point_in_rect(int x, int y, int rx, int ry, int rw, int rh) {
    return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

typedef struct {
    int x, y, w, h;
    const char *label;
    bool hovered;
} Button;

static bool button_update(Button *b, int tx, int ty, bool just_released) {
    bool inside = point_in_rect(tx, ty, b->x, b->y, b->w, b->h);
    b->hovered = inside;
    if (just_released && inside) {
        return true;
    }
    return false;
}

static void draw_button(SDL_Renderer *r, Button *b) {
    uint8_t br = 40, bg = 60, bb_ = 80;
    if (b->hovered) { br = 0; bg = 160; bb_ = 255; }
    fill_rect(r, b->x, b->y, b->w, b->h, br, bg, bb_, 255);
    draw_rect_outline(r, b->x, b->y, b->w, b->h, 140, 180, 220);
    int tw = (int)strlen(b->label) * 21;
    int tx = b->x + (b->w - tw) / 2;
    int ty = b->y + (b->h - 24) / 2;
    draw_text(r, tx, ty, b->label, 255, 255, 255);
}

static void draw_panel(SDL_Renderer *r, int x, int y, int w, int h, const char *title) {
    fill_rect(r, x, y, w, h, 22, 30, 40, 255);
    draw_rect_outline(r, x, y, w, h, 60, 80, 110);
    fill_rect(r, x, y, w, 30, 10, 50, 90, 255);
    draw_rect_outline(r, x, y, w, 30, 100, 150, 200);
    draw_text(r, x + 12, y + 8, title, 255, 255, 255);
}

static void draw_status_dot(SDL_Renderer *r, int x, int y, bool active) {
    fill_rect(r, x, y, 16, 16, active ? 0 : 100, active ? 255 : 60, active ? 0 : 60, 255);
    draw_rect_outline(r, x, y, 16, 16, 200, 200, 200);
}

static void draw_progress_bar(SDL_Renderer *r, int x, int y, int w, int h, float pct, uint8_t rr, uint8_t gg, uint8_t bb) {
    fill_rect(r, x, y, w, h, 10, 16, 24, 255);
    draw_rect_outline(r, x, y, w, h, 60, 80, 110);
    int fill = (int)((float)(w - 4) * pct);
    if (fill > 0) {
        fill_rect(r, x + 2, y + 2, fill, h - 4, rr, gg, bb, 255);
    }
}

static void draw_hashrate_graph(SDL_Renderer *r, int x, int y, int w, int h, const double *history, int count, double max_val) {
    fill_rect(r, x, y, w, h, 10, 16, 24, 255);
    draw_rect_outline(r, x, y, w, h, 60, 80, 110);
    if (max_val <= 0) max_val = 1.0;
    int px = x;
    for (int i = 0; i < count && i < HASHRATE_HISTORY_MAX; i++) {
        if (history[i] <= 0) continue;
        int bh = (int)((history[i] / max_val) * (h - 4));
        if (bh < 1) bh = 1;
        if (bh > h - 4) bh = h - 4;
        fill_rect(r, px, y + h - 4 - bh, 3, bh, 0, 255, 140, 255);
        px += 4;
        if (px > x + w - 4) break;
    }
}

static void draw_input_field(SDL_Renderer *r, int x, int y, int w, const char *text, bool active);

void ui_log(const char *fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    for (int i = 7; i > 0; i--) {
        snprintf(g_log_lines[i], 96, "%s", g_log_lines[i - 1]);
    }
    snprintf(g_log_lines[0], 96, "%s", buf);
    if (g_log_count < 8) g_log_count++;
}

int ui_init(void) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        return -1;
    }
    g_win = SDL_CreateWindow("VitaDucoMiner", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 960, 544, SDL_WINDOW_SHOWN);
    if (!g_win) { SDL_Quit(); return -1; }
    g_rend = SDL_CreateRenderer(g_win, -1, SDL_RENDERER_ACCELERATED);
    if (!g_rend) { SDL_DestroyWindow(g_win); g_win = NULL; SDL_Quit(); return -1; }
    SDL_SetRenderDrawBlendMode(g_rend, SDL_BLENDMODE_BLEND);
    SDL_StartTextInput();
    return 0;
}

void ui_quit(void) {
    SDL_StopTextInput();
    if (g_rend) SDL_DestroyRenderer(g_rend);
    if (g_win) SDL_DestroyWindow(g_win);
    g_rend = NULL;
    g_win = NULL;
    SDL_Quit();
}

int ui_poll_events(bool *quit, int *touch_x, int *touch_y, bool *touch_pressed, char *text_in, int text_in_size) {
    *quit = false;
    *touch_pressed = false;
    g_touch_just_released = false;
    *touch_x = g_touch_x;
    *touch_y = g_touch_y;
    g_text_in[0] = 0;
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
            case SDL_QUIT: *quit = true; break;
            case SDL_MOUSEBUTTONDOWN:
            case SDL_FINGERDOWN:
                g_touch_x = (ev.type == SDL_MOUSEBUTTONDOWN) ? ev.button.x : (int)(ev.tfinger.x * 960.0f);
                g_touch_y = (ev.type == SDL_MOUSEBUTTONDOWN) ? ev.button.y : (int)(ev.tfinger.y * 544.0f);
                g_touch_pressed = true;
                break;
            case SDL_MOUSEBUTTONUP:
            case SDL_FINGERUP:
                g_touch_x = (ev.type == SDL_MOUSEBUTTONUP) ? ev.button.x : (int)(ev.tfinger.x * 960.0f);
                g_touch_y = (ev.type == SDL_MOUSEBUTTONUP) ? ev.button.y : (int)(ev.tfinger.y * 544.0f);
                g_touch_pressed = false;
                g_touch_just_released = true;
                break;
            case SDL_TEXTINPUT:
                if (g_active_field >= 0) {
                    strncat(g_text_in, ev.text.text, sizeof(g_text_in) - 1 - strlen(g_text_in));
                }
                break;
            case SDL_KEYDOWN:
                if (g_active_field >= 0 && ev.key.keysym.sym == SDLK_BACKSPACE) {
                    size_t len = strlen(g_text_in);
                    if (len > 0) g_text_in[len - 1] = 0;
                }
                break;
        }
    }
    *touch_x = g_touch_x;
    *touch_y = g_touch_y;
    *touch_pressed = g_touch_pressed;
    strncpy(text_in, g_text_in, text_in_size - 1);
    text_in[text_in_size - 1] = 0;
    return 0;
}

static const char *field_buf_for(int field) {
    if (field == 0) return g_set_server;
    if (field == 1) return g_set_port;
    if (field == 2) return g_set_user;
    if (field == 3) return g_set_worker;
    return g_set_key;
}
static char *field_buf_for_write(int field) {
    if (field == 0) return g_set_server;
    if (field == 1) return g_set_port;
    if (field == 2) return g_set_user;
    if (field == 3) return g_set_worker;
    if (field == 4) return g_set_key;
    if (field == 5) return g_set_donation;
    return g_set_server;
}
static int field_max_len(int field) {
    if (field == 1) return 8;
    if (field == 5) return 64;
    return field == 3 ? 64 : 128;
}

void ui_commit_active_field(void) {
    if (g_active_field >= 0) {
        snprintf(field_buf_for_write(g_active_field), field_max_len(g_active_field), "%s", g_text_in);
        g_settings_dirty = true;
    }
}

void ui_set_active_field(int field) {
    g_active_field = field;
    if (g_active_field >= 0) {
        snprintf(g_text_in, sizeof(g_text_in), "%s", field_buf_for(g_active_field));
    } else {
        g_text_in[0] = 0;
    }
}

int ui_get_active_field(void) { return g_active_field; }

void ui_set_server_text(const char *text) { snprintf(g_set_server, sizeof(g_set_server), "%s", text); }
void ui_set_port_text(const char *text) { snprintf(g_set_port, sizeof(g_set_port), "%s", text); }
void ui_set_user_text(const char *text) { snprintf(g_set_user, sizeof(g_set_user), "%s", text); }
void ui_set_worker_text(const char *text) { snprintf(g_set_worker, sizeof(g_set_worker), "%s", text); }
void ui_set_key_text(const char *text) { snprintf(g_set_key, sizeof(g_set_key), "%s", text); }
void ui_set_donation_text(const char *text) { snprintf(g_set_donation, sizeof(g_set_donation), "%s", text); }

void ui_set_auto_start(bool enabled) { g_set_auto_start = enabled; }
bool ui_get_auto_start(void) { return g_set_auto_start; }

bool ui_settings_dirty(void) { return g_settings_dirty; }
void ui_settings_clear_dirty(void) { g_settings_dirty = false; }
const char *ui_get_server(void) { return g_set_server; }
const char *ui_get_port(void) { return g_set_port; }
const char *ui_get_user(void) { return g_set_user; }
const char *ui_get_worker(void) { return g_set_worker; }
const char *ui_get_key(void) { return g_set_key; }
const char *ui_get_donation_addr(void) { return g_set_donation; }

UiAction ui_render(const UiStats *stats, UiScreen screen, int touch_x, int touch_y, bool touch_pressed, int active_field) {
    (void)touch_pressed;
    g_active_field = active_field;
    g_cursor_blink++;
    fill_rect(g_rend, 0, 0, 960, 544, 14, 20, 28, 255);

    fill_rect(g_rend, 0, 0, 960, 56, 20, 40, 60, 255);
    draw_rect_outline(g_rend, 0, 0, 960, 56, 80, 120, 160);
    draw_text(g_rend, 20, 16, "VITA DUCO MINER", 255, 255, 255);
    draw_text_small(g_rend, 20, 36, "DuinoCoin GUI Miner  PSVita", 160, 190, 220);

    draw_status_dot(g_rend, 800, 20, stats->connected);
    draw_text(g_rend, 824, 16, stats->connected ? "ONLINE" : "OFFLINE", stats->connected ? 0 : 255, stats->connected ? 255 : 0, 0);

    if (screen == SCREEN_DASHBOARD) {
        draw_panel(g_rend, 20, 72, 548, 388, "MINING STATUS");

        int cx = 50, cy = 112;
        draw_text(g_rend, cx, cy, "HASHRATE", 150, 180, 220);
        char hbuf[32];
        snprintf(hbuf, sizeof(hbuf), "%.1f H/s", stats->hashrate);
        draw_text(g_rend, cx, cy + 28, hbuf, 0, 255, 140);

        draw_progress_bar(g_rend, cx, cy + 66, 200, 14, stats->hashrate > 0 ? 0.5f : 0.0f, 0, 200, 100);

        double max_hr = 0;
        for (int i = 0; i < HASHRATE_HISTORY_MAX; i++) {
            if (stats->hashrate_history[i] > max_hr) max_hr = stats->hashrate_history[i];
        }
        draw_hashrate_graph(g_rend, cx, cy + 90, 280, 70, stats->hashrate_history, HASHRATE_HISTORY_MAX, max_hr);
        draw_text_small(g_rend, cx, cy + 162, "HASHRATE (60s)", 120, 150, 180);

        cy = 290;
        draw_text(g_rend, cx, cy, "SHARES", 150, 180, 220);
        snprintf(hbuf, sizeof(hbuf), "ACCEPTED  %llu", stats->accepted);
        draw_text(g_rend, cx, cy + 26, hbuf, 100, 255, 100);
        snprintf(hbuf, sizeof(hbuf), "REJECTED  %llu", stats->rejected);
        draw_text(g_rend, cx, cy + 52, hbuf, 255, 100, 100);

        cy += 90;
        draw_text(g_rend, cx, cy, "DIFFICULTY", 150, 180, 220);
        snprintf(hbuf, sizeof(hbuf), "%u", stats->difficulty);
        draw_text(g_rend, cx, cy + 26, hbuf, 220, 220, 220);

        cy += 52;
        draw_text(g_rend, cx, cy, "LAST NONCE", 150, 180, 220);
        snprintf(hbuf, sizeof(hbuf), "%u", stats->nonce);
        draw_text(g_rend, cx, cy + 26, hbuf, 220, 220, 220);

        cy += 52;
        draw_text(g_rend, cx, cy, "UPTIME", 150, 180, 220);
        uint64_t up = stats->uptime_ms;
        uint32_t hh = (uint32_t)(up / 3600000ull);
        uint32_t mm = (uint32_t)((up % 3600000ull) / 60000ull);
        uint32_t ss = (uint32_t)((up % 60000ull) / 1000ull);
        snprintf(hbuf, sizeof(hbuf), "%02lu:%02lu:%02lu", (unsigned long)hh, (unsigned long)mm, (unsigned long)ss);
        draw_text(g_rend, cx, cy + 26, hbuf, 220, 220, 220);

        if (stats->reject_reason && stats->reject_reason[0]) {
            cy += 78;
            draw_text(g_rend, cx, cy, "LAST REJECT:", 255, 100, 100);
            draw_text(g_rend, cx, cy + 26, stats->reject_reason, 255, 150, 150);
        }

        Button btn = { 110, 360, 320, 64, stats->mining ? "STOP MINING" : "START MINING", false };
        if (button_update(&btn, touch_x, touch_y, g_touch_just_released)) return UI_ACTION_TOGGLE_MINING;
        draw_button(g_rend, &btn);

        Button btn_auto = { 450, 360, 180, 64, stats->auto_start ? "AUTO: ON" : "AUTO: OFF", false };
        if (button_update(&btn_auto, touch_x, touch_y, g_touch_just_released)) return UI_ACTION_TOGGLE_AUTO_START;
        draw_button(g_rend, &btn_auto);

        draw_panel(g_rend, 588, 72, 352, 388, "CONNECTION");
        cx = 618; cy = 112;
        draw_text(g_rend, cx, cy, "SERVER", 150, 180, 220);
        draw_text(g_rend, cx, cy + 28, stats->server, 220, 230, 240);
        snprintf(hbuf, sizeof(hbuf), ":%u", stats->port);
        draw_text(g_rend, cx + (int)strlen(stats->server) * 21, cy + 28, hbuf, 220, 230, 240);

        cy += 70;
        draw_text(g_rend, cx, cy, "USERNAME", 150, 180, 220);
        draw_text(g_rend, cx, cy + 28, stats->username, 220, 230, 240);
        if (stats->worker && stats->worker[0]) {
            snprintf(hbuf, sizeof(hbuf), ".%s", stats->worker);
            draw_text(g_rend, cx + (int)strlen(stats->username) * 21, cy + 28, hbuf, 160, 190, 220);
        }

        cy += 70;
        draw_text(g_rend, cx, cy, "LOCAL IP", 150, 180, 220);
        draw_text(g_rend, cx, cy + 28, stats->ip ? stats->ip : "0.0.0.0", 220, 230, 240);

        cy += 70;
        draw_text(g_rend, cx, cy, "HASHRATE", 150, 180, 220);
        snprintf(hbuf, sizeof(hbuf), "%.1f H/s", stats->hashrate);
        draw_text(g_rend, cx, cy + 28, hbuf, 220, 230, 240);

        cy += 70;
        draw_text(g_rend, cx, cy, stats->pool_valid ? "POOL (auto)" : "POOL", 150, 180, 220);
        snprintf(hbuf, sizeof(hbuf), "%s:%u", stats->pool_host ? stats->pool_host : "server.duinocoin.com", stats->pool_port);
        draw_text(g_rend, cx, cy + 28, hbuf, stats->pool_valid ? 100 : 200, stats->pool_valid ? 255 : 230, stats->pool_valid ? 100 : 240);

        if (stats->pool_fetching) {
            cy += 56;
            draw_text(g_rend, cx, cy, "FETCHING...", 255, 255, 100);
        }

        cy += 82;
        draw_text(g_rend, cx, cy, "ACTIVITY LOG", 150, 180, 220);
        for (int i = 0; i < 5 && i < g_log_count; i++) {
            draw_text_small(g_rend, cx, cy + 26 + i * 22, g_log_lines[i], 150, 170, 190);
        }

        Button bset = { 30, 472, 180, 60, "SETTINGS", false };
        Button babt = { 230, 472, 180, 60, "ABOUT", false };
        Button bdon = { 430, 472, 180, 60, "DONATE", false };
        Button bquit = { 750, 472, 180, 60, "QUIT", false };
        if (button_update(&bset, touch_x, touch_y, g_touch_just_released)) return UI_ACTION_OPEN_SETTINGS;
        if (button_update(&babt, touch_x, touch_y, g_touch_just_released)) return UI_ACTION_OPEN_ABOUT;
        if (button_update(&bdon, touch_x, touch_y, g_touch_just_released)) return UI_ACTION_OPEN_DONATION;
        if (button_update(&bquit, touch_x, touch_y, g_touch_just_released)) return UI_ACTION_QUIT;
        draw_button(g_rend, &bset);
        draw_button(g_rend, &babt);
        draw_button(g_rend, &bdon);
        draw_button(g_rend, &bquit);

    } else if (screen == SCREEN_SETTINGS) {
        draw_panel(g_rend, 20, 72, 920, 388, "SETTINGS");
        int cx = 60, cy = 100;

        draw_text(g_rend, cx, cy, "SERVER", 150, 180, 220);
        bool f0 = (g_active_field == 0);
        draw_input_field(g_rend, cx, cy + 28, 400, f0 ? g_text_in : g_set_server, f0);
        fill_rect(g_rend, cx, cy + 54, 400, 2, 80, 110, 150, 255);

        cy += 80;
        draw_text(g_rend, cx, cy, "PORT", 150, 180, 220);
        bool f1 = (g_active_field == 1);
        draw_input_field(g_rend, cx, cy + 28, 120, f1 ? g_text_in : g_set_port, f1);
        fill_rect(g_rend, cx, cy + 54, 120, 2, 80, 110, 150, 255);

        cy += 80;
        draw_text(g_rend, cx, cy, "USERNAME", 150, 180, 220);
        bool f2 = (g_active_field == 2);
        draw_input_field(g_rend, cx, cy + 28, 300, f2 ? g_text_in : g_set_user, f2);
        fill_rect(g_rend, cx, cy + 54, 300, 2, 80, 110, 150, 255);

        cy += 80;
        draw_text(g_rend, cx, cy, "WORKER", 150, 180, 220);
        bool f3 = (g_active_field == 3);
        draw_input_field(g_rend, cx, cy + 28, 300, f3 ? g_text_in : g_set_worker, f3);
        fill_rect(g_rend, cx, cy + 54, 300, 2, 80, 110, 150, 255);

        cy += 80;
        draw_text(g_rend, cx, cy, "MINING KEY", 150, 180, 220);
        bool f4 = (g_active_field == 4);
        draw_input_field(g_rend, cx, cy + 28, 400, f4 ? g_text_in : g_set_key, f4);
        fill_rect(g_rend, cx, cy + 54, 400, 2, 80, 110, 150, 255);

        cy += 80;
        draw_text(g_rend, cx, cy, "DONATION ADDR", 150, 180, 220);
        bool f5 = (g_active_field == 5);
        draw_input_field(g_rend, cx, cy + 28, 400, f5 ? g_text_in : g_set_donation, f5);
        fill_rect(g_rend, cx, cy + 54, 400, 2, 80, 110, 150, 255);

        Button bfetch = { 500, 370, 260, 64, "FETCH POOL", false };
        Button bsave = { 140, 450, 240, 64, "SAVE", false };
        Button bback = { 420, 450, 240, 64, "BACK", false };
        if (button_update(&bfetch, touch_x, touch_y, g_touch_just_released)) return UI_ACTION_FETCH_POOL;
        if (button_update(&bsave, touch_x, touch_y, g_touch_just_released)) return UI_ACTION_SAVE_SETTINGS;
        if (button_update(&bback, touch_x, touch_y, g_touch_just_released)) return UI_ACTION_BACK;
        draw_button(g_rend, &bfetch);
        draw_button(g_rend, &bsave);
        draw_button(g_rend, &bback);

    } else if (screen == SCREEN_ABOUT) {
        draw_panel(g_rend, 20, 72, 920, 388, "ABOUT");
        int cx = 60, cy = 120;
        draw_text(g_rend, cx, cy, "VitaDucoMiner", 255, 255, 255);
        cy += 40;
        draw_text(g_rend, cx, cy, "DuinoCoin Miner for PSVita", 220, 230, 240);
        cy += 40;
        draw_text(g_rend, cx, cy, "GUI  SDL2  VitaSDK", 170, 190, 210);
        cy += 40;
        draw_text(g_rend, cx, cy, "Algorithm: DUCO-S1 (SHA-1)", 170, 190, 210);
        cy += 40;
        draw_text(g_rend, cx, cy, "Unofficial DuinoCoin miner", 150, 170, 190);
        cy += 80;
        Button bback = { 360, 360, 240, 64, "BACK", false };
        if (button_update(&bback, touch_x, touch_y, g_touch_just_released)) return UI_ACTION_BACK;
        draw_button(g_rend, &bback);

    } else if (screen == SCREEN_DONATION) {
        draw_panel(g_rend, 20, 72, 920, 388, "DONATE");
        int cx = 60, cy = 120;
        draw_text(g_rend, cx, cy, "Support VitaDucoMiner", 255, 255, 255);
        cy += 40;
        draw_text(g_rend, cx, cy, "DuinoCoin Address:", 220, 230, 240);
        cy += 30;
        draw_text(g_rend, cx, cy, stats->donation_addr ? stats->donation_addr : "(none set)", 0, 255, 140);
        cy += 60;
        draw_text(g_rend, cx, cy, "Tap to copy / set in Settings", 150, 170, 190);
        cy += 80;
        Button bback = { 360, 360, 240, 64, "BACK", false };
        if (button_update(&bback, touch_x, touch_y, g_touch_just_released)) return UI_ACTION_BACK;
        draw_button(g_rend, &bback);
    }

    SDL_RenderPresent(g_rend);
    return UI_ACTION_NONE;
}

static void draw_input_field(SDL_Renderer *r, int x, int y, int w, const char *text, bool active) {
    fill_rect(r, x, y, w, 26, 10, 16, 24, 255);
    draw_rect_outline(r, x, y, w, 26, active ? 0 : 60, active ? 160 : 80, active ? 255 : 110);
    draw_text(r, x + 8, y + 6, text, 255, 255, 255);
    if (active && (g_cursor_blink / 30) % 2 == 0) {
        int tx = x + 8 + (int)strlen(text) * 21;
        fill_rect(r, tx, y + 6, 3, 16, 255, 255, 255, 255);
    }
}

const char *ui_screen_name(UiScreen screen) {
    switch (screen) {
        case SCREEN_DASHBOARD: return "dashboard";
        case SCREEN_SETTINGS: return "settings";
        case SCREEN_ABOUT: return "about";
        case SCREEN_DONATION: return "donation";
    }
    return "unknown";
}
