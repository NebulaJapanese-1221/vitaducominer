#ifndef UI_H
#define UI_H

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdint.h>

#define HASHRATE_HISTORY_MAX 60

typedef struct {
    bool connected;
    bool mining;
    bool auto_start;
    const char *ip;
    const char *server;
    uint16_t port;
    const char *username;
    const char *worker;
    const char *mining_key;
    const char *reject_reason;
    uint32_t difficulty;
    uint64_t hashes;
    double hashrate;
    double hashrate_history[HASHRATE_HISTORY_MAX];
    uint64_t accepted;
    uint64_t rejected;
    uint32_t nonce;
    uint64_t uptime_ms;
    const char *pool_host;
    uint16_t pool_port;
    bool pool_valid;
    bool pool_fetching;
    const char *donation_addr;
} UiStats;

typedef enum {
    SCREEN_DASHBOARD,
    SCREEN_SETTINGS,
    SCREEN_ABOUT,
    SCREEN_DONATION
} UiScreen;

typedef enum {
    UI_ACTION_NONE,
    UI_ACTION_TOGGLE_MINING,
    UI_ACTION_TOGGLE_AUTO_START,
    UI_ACTION_OPEN_SETTINGS,
    UI_ACTION_OPEN_ABOUT,
    UI_ACTION_OPEN_DONATION,
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
const char *ui_get_worker(void);
const char *ui_get_key(void);
const char *ui_get_donation_addr(void);
void ui_set_active_field(int field);
int ui_get_active_field(void);
void ui_commit_active_field(void);
void ui_set_server_text(const char *text);
void ui_set_port_text(const char *text);
void ui_set_user_text(const char *text);
void ui_set_worker_text(const char *text);
void ui_set_key_text(const char *text);
void ui_set_donation_text(const char *text);
void ui_set_auto_start(bool enabled);
bool ui_get_auto_start(void);

#endif
