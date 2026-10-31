#ifndef UI_H
#define UI_H

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool connected;
    bool mining;
    const char *ip;
    const char *server;
    uint16_t port;
    const char *username;
    const char *mining_key;
    uint32_t difficulty;
    uint64_t hashes;
    double hashrate;
    uint64_t accepted;
    uint64_t rejected;
    uint32_t nonce;
    uint64_t uptime_ms;
    const char *pool_host;
    uint16_t pool_port;
    bool pool_valid;
} UiStats;

typedef enum {
    SCREEN_DASHBOARD,
    SCREEN_SETTINGS,
    SCREEN_ABOUT
} UiScreen;

typedef enum {
    UI_ACTION_NONE,
    UI_ACTION_TOGGLE_MINING,
    UI_ACTION_OPEN_SETTINGS,
    UI_ACTION_OPEN_ABOUT,
    UI_ACTION_BACK,
    UI_ACTION_QUIT,
    UI_ACTION_SAVE_SETTINGS,
    UI_ACTION_FETCH_POOL
} UiAction;

int ui_init(void);
void ui_quit(void);
int ui_poll_events(bool *quit, int *touch_x, int *touch_y, bool *touch_pressed, char *text_in, int text_in_size);
UiAction ui_render(const UiStats *stats, UiScreen screen, int touch_x, int touch_y, bool touch_pressed, int active_field);
const char *ui_screen_name(UiScreen screen);
void ui_log(const char *fmt, ...);
bool ui_settings_dirty(void);
void ui_settings_clear_dirty(void);
const char *ui_get_server(void);
const char *ui_get_port(void);
const char *ui_get_user(void);
const char *ui_get_key(void);
void ui_set_active_field(int field);
int ui_get_active_field(void);
void ui_commit_active_field(void);
void ui_set_server_text(const char *text);
void ui_set_port_text(const char *text);
void ui_set_user_text(const char *text);

#endif
