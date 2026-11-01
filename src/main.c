#include "miner.h"
#include "net.h"
#include "config.h"
#include "pool.h"
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/sysmodule.h>
#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <vita2d.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#define SCREEN_W 960
#define SCREEN_H 544

typedef enum {
    SCREEN_DASHBOARD,
    SCREEN_SETTINGS,
    SCREEN_SETTINGS_EDIT,
    SCREEN_ABOUT,
    SCREEN_DONATION,
    SCREEN_LOG
} UiScreen;

static UiScreen g_screen = SCREEN_DASHBOARD;
static int g_sel = 0;
static int g_edit_field = 0;
static char g_text_in[256] = {0};

static bool g_prev_cross = false;
static bool g_prev_circle = false;
static bool g_prev_square = false;
static bool g_prev_up = false;
static bool g_prev_down = false;
static bool g_prev_left = false;
static bool g_prev_right = false;

static uint64_t g_last_stats = 0;
static double g_cached_hashrate = 0.0;
static char g_ip[16] = {0};

static Config g_cfg;

static vita2d_pgf *g_font = NULL;

static uint64_t g_button_cooldown = 0;

static int btn_pressed(unsigned int cur, unsigned int prev, unsigned int mask) {
    return (cur & mask) && !(prev & mask);
}

/* Touch state */
static int g_touch_x = 0;
static int g_touch_y = 0;
static bool g_touch_pressed = false;

static void update_touch(void) {
    SceTouchData touch;
    memset(&touch, 0, sizeof(touch));
    sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1);
    if (touch.reportNum > 0) {
        g_touch_x = touch.report[0].x * SCREEN_W / 1920;
        g_touch_y = touch.report[0].y * SCREEN_H / 1088;
        g_touch_pressed = true;
    } else {
        g_touch_pressed = false;
    }
}

static int point_in_rect(int x, int y, int rx, int ry, int rw, int rh) {
    return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

typedef struct {
    int x, y, w, h;
    const char *label;
    bool hovered;
} Button;

static bool button_update(Button *b, bool just_pressed) {
    bool inside = point_in_rect(g_touch_x, g_touch_y, b->x, b->y, b->w, b->h);
    b->hovered = inside;
    if (just_pressed && inside) {
        return true;
    }
    return false;
}

static void draw_button(Button *b) {
    unsigned int col = b->hovered ? 0xFF007ACC : 0xFF2A3B4C;
    unsigned int outline = 0xFF556B7F;
    vita2d_draw_rectangle(b->x, b->y, b->w, b->h, col);
    vita2d_draw_rectangle(b->x, b->y, b->w, 2, outline);
    vita2d_draw_rectangle(b->x, b->y + b->h - 2, b->w, 2, outline);
    vita2d_draw_rectangle(b->x, b->y, 2, b->h, outline);
    vita2d_draw_rectangle(b->x + b->w - 2, b->y, 2, b->h, outline);

    int tw = (int)strlen(b->label) * 12;
    int tx = b->x + (b->w - tw) / 2;
    int ty = b->y + (b->h - 18) / 2;
    vita2d_pgf_draw_text(g_font, tx, ty, 0xFFFFFFFF, 1.0f, b->label);
}

static void draw_panel(int x, int y, int w, int h, const char *title) {
    vita2d_draw_rectangle(x, y, w, h, 0xFF1E2A36);
    vita2d_draw_rectangle(x, y, w, h, 0xFF3A4B5C);
    vita2d_draw_rectangle(x, y + h - 2, w, 2, 0xFF3A4B5C);
    vita2d_draw_rectangle(x, y, 2, h, 0xFF3A4B5C);
    vita2d_draw_rectangle(x + w - 2, y, 2, h, 0xFF3A4B5C);
    vita2d_draw_rectangle(x, y, w, 30, 0xFF0E1420);
    vita2d_draw_rectangle(x, y + 28, w, 2, 0xFF3A4B5C);
    vita2d_pgf_draw_text(g_font, x + 12, y + 6, 0xFFFFFFFF, 1.0f, title);
}

static void draw_status_dot(int x, int y, bool active) {
    vita2d_draw_rectangle(x, y, 16, 16, active ? 0xFF00FF00 : 0xFFFF0000);
    vita2d_draw_rectangle(x, y, 16, 16, 0xFF556B7F);
}

static void draw_progress_bar(int x, int y, int w, int h, float pct, unsigned int color) {
    vita2d_draw_rectangle(x, y, w, h, 0xFF0A1218);
    vita2d_draw_rectangle(x, y, w, h, 0xFF3A4B5C);
    int fill = (int)((float)(w - 4) * pct);
    if (fill > 0) {
        vita2d_draw_rectangle(x + 2, y + 2, fill, h - 4, color);
    }
}

static void draw_textf(int x, int y, unsigned int color, float scale, const char *fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (g_font) {
        vita2d_pgf_draw_text(g_font, x, y, color, scale, buf);
    }
}

static char g_log_lines[8][96];
static int g_log_count = 0;

void config_log(const char *fmt, ...); /* forward - defined in config.c */

static void ui_log(const char *fmt, ...) {
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

static void render_dashboard(void) {
    Miner *m = miner_get_stats();

    vita2d_start_drawing();
    vita2d_draw_rectangle(0, 0, SCREEN_W, SCREEN_H, 0xFF0E1420);

    /* Header */
    vita2d_draw_rectangle(0, 0, SCREEN_W, 56, 0xFF16242E);
    vita2d_draw_rectangle(0, 56, SCREEN_W, 2, 0xFF3A4B5C);
    vita2d_pgf_draw_text(g_font, 20, 16, 0xFFFFFFFF, 1.2f, "VITA DUCO MINER");
    vita2d_pgf_draw_text(g_font, 20, 36, 0xFF808080, 0.7f, "DuinoCoin GUI Miner  PSVita");

    draw_status_dot(800, 20, m->connected);
    vita2d_pgf_draw_text(g_font, 824, 16, m->connected ? 0xFF00FF00 : 0xFFFF0000, 1.0f,
                         m->connected ? "ONLINE" : "OFFLINE");

    /* Left panel: Mining Status */
    draw_panel(20, 72, 548, 388, "MINING STATUS");

    int cx = 50, cy = 112;
    vita2d_pgf_draw_text(g_font, cx, cy, 0xFF80B3D6, 0.8f, "HASHRATE");
    char hbuf[32];
    snprintf(hbuf, sizeof(hbuf), "%.1f H/s", g_cached_hashrate);
    vita2d_pgf_draw_text(g_font, cx, cy + 28, 0xFF00FF80, 1.2f, hbuf);

    draw_progress_bar(cx, cy + 66, 200, 14, g_cached_hashrate > 0 ? 0.5f : 0.0f, 0xFF00FF80);

    cy = 220;
    vita2d_pgf_draw_text(g_font, cx, cy, 0xFF80B3D6, 0.8f, "SHARES");
    snprintf(hbuf, sizeof(hbuf), "ACCEPTED  %llu", (unsigned long long)m->shares_accepted);
    vita2d_pgf_draw_text(g_font, cx, cy + 28, 0xFF00FF80, 0.8f, hbuf);
    snprintf(hbuf, sizeof(hbuf), "REJECTED  %llu", (unsigned long long)m->shares_rejected);
    vita2d_pgf_draw_text(g_font, cx, cy + 56, 0xFFFF5555, 0.8f, hbuf);

    cy = 320;
    vita2d_pgf_draw_text(g_font, cx, cy, 0xFF80B3D6, 0.8f, "DIFFICULTY");
    snprintf(hbuf, sizeof(hbuf), "%u", m->difficulty);
    vita2d_pgf_draw_text(g_font, cx, cy + 28, 0xFFC0C0C0, 0.9f, hbuf);

    cy = 360;
    vita2d_pgf_draw_text(g_font, cx, cy, 0xFF80B3D6, 0.8f, "LAST NONCE");
    snprintf(hbuf, sizeof(hbuf), "%u", m->nonce);
    vita2d_pgf_draw_text(g_font, cx, cy + 28, 0xFFC0C0C0, 0.9f, hbuf);

    /* Right panel: Connection */
    draw_panel(588, 72, 352, 388, "CONNECTION");
    cx = 618; cy = 112;
    vita2d_pgf_draw_text(g_font, cx, cy, 0xFF80B3D6, 0.8f, "SERVER");
    vita2d_pgf_draw_text(g_font, cx, cy + 28, 0xFFC0C0C0, 0.8f, m->server);
    snprintf(hbuf, sizeof(hbuf), ":%u", m->port);
    vita2d_pgf_draw_text(g_font, cx + (int)strlen(m->server) * 12, cy + 28, 0xFFC0C0C0, 0.8f, hbuf);

    cy += 70;
    vita2d_pgf_draw_text(g_font, cx, cy, 0xFF80B3D6, 0.8f, "USERNAME");
    vita2d_pgf_draw_text(g_font, cx, cy + 28, 0xFFC0C0C0, 0.8f, m->username);
    if (m->worker[0]) {
        snprintf(hbuf, sizeof(hbuf), ".%s", m->worker);
        vita2d_pgf_draw_text(g_font, cx + (int)strlen(m->username) * 12, cy + 28, 0xFF80B3D6, 0.8f, hbuf);
    }

    cy += 70;
    vita2d_pgf_draw_text(g_font, cx, cy, 0xFF80B3D6, 0.8f, "LOCAL IP");
    vita2d_pgf_draw_text(g_font, cx, cy + 28, 0xFFC0C0C0, 0.8f, g_ip[0] ? g_ip : "0.0.0.0");

    cy += 70;
    vita2d_pgf_draw_text(g_font, cx, cy, 0xFF80B3D6, 0.8f, "HASHRATE");
    snprintf(hbuf, sizeof(hbuf), "%.1f H/s", g_cached_hashrate);
    vita2d_pgf_draw_text(g_font, cx, cy + 28, 0xFFC0C0C0, 0.8f, hbuf);

    cy += 70;
    vita2d_pgf_draw_text(g_font, cx, cy, 0xFF80B3D6, 0.8f, "POOL");
    snprintf(hbuf, sizeof(hbuf), "%s:%u", miner_pool_host() ? miner_pool_host() : "server.duinocoin.com",
             miner_pool_port());
    vita2d_pgf_draw_text(g_font, cx, cy + 28, miner_pool_valid() ? 0xFF00FF80 : 0xFFC0C0C0, 0.8f, hbuf);

    if (m->reject_reason[0]) {
        cy += 70;
        vita2d_pgf_draw_text(g_font, cx, cy, 0xFFFF5555, 0.7f, "REJECT:");
        vita2d_pgf_draw_text(g_font, cx, cy + 28, 0xFFFF8080, 0.7f, m->reject_reason);
    }

    /* Bottom buttons */
    Button btn_mine = { 110, 472, 260, 64, m->mining ? "STOP MINING" : "START MINING" };
    Button btn_set = { 390, 472, 180, 64, "SETTINGS" };
    Button btn_abt = { 590, 472, 180, 64, "ABOUT" };
    Button btn_quit = { 790, 472, 150, 64, "QUIT" };

    if (button_update(&btn_mine, g_touch_pressed)) {
        miner_toggle_mining();
        ui_log("%s", miner_get_stats()->mining ? "Mining started" : "Mining stopped");
        g_button_cooldown = sceKernelGetSystemTimeWide();
    }
    if (button_update(&btn_set, g_touch_pressed)) {
        g_screen = SCREEN_SETTINGS;
        g_sel = 0;
        g_button_cooldown = sceKernelGetSystemTimeWide();
    }
    if (button_update(&btn_abt, g_touch_pressed)) {
        g_screen = SCREEN_ABOUT;
        g_button_cooldown = sceKernelGetSystemTimeWide();
    }
    if (button_update(&btn_quit, g_touch_pressed)) {
        g_button_cooldown = sceKernelGetSystemTimeWide();
        /* handled in main loop */
    }

    draw_button(&btn_mine);
    draw_button(&btn_set);
    draw_button(&btn_abt);
    draw_button(&btn_quit);

    vita2d_end_drawing();
    vita2d_swap_buffers();
}

static void render_settings(void) {
    char hbuf[32];
    vita2d_start_drawing();
    vita2d_draw_rectangle(0, 0, SCREEN_W, SCREEN_H, 0xFF0E1420);

    vita2d_draw_rectangle(0, 0, SCREEN_W, 56, 0xFF16242E);
    vita2d_draw_rectangle(0, 56, SCREEN_W, 2, 0xFF3A4B5C);
    vita2d_pgf_draw_text(g_font, 20, 16, 0xFFFFFFFF, 1.2f, "SETTINGS");

    int cx = 60, cy = 100;

    vita2d_pgf_draw_text(g_font, cx, cy, 0xFF80B3D6, 0.8f, "SERVER");
    vita2d_draw_rectangle(cx, cy + 28, 400, 26, g_sel == 0 ? 0xFF005577 : 0xFF0A1218);
    vita2d_draw_rectangle(cx, cy + 28, 400, 26, g_sel == 0 ? 0xFF00AAFF : 0xFF3A4B5C);
    vita2d_pgf_draw_text(g_font, cx + 8, cy + 32, 0xFFFFFFFF, 0.8f, g_set_server);

    cy += 70;
    vita2d_pgf_draw_text(g_font, cx, cy, 0xFF80B3D6, 0.8f, "PORT");
    vita2d_draw_rectangle(cx, cy + 28, 120, 26, g_sel == 1 ? 0xFF005577 : 0xFF0A1218);
    vita2d_draw_rectangle(cx, cy + 28, 120, 26, g_sel == 1 ? 0xFF00AAFF : 0xFF3A4B5C);
    vita2d_pgf_draw_text(g_font, cx + 8, cy + 32, 0xFFFFFFFF, 0.8f, g_set_port);

    cy += 70;
    vita2d_pgf_draw_text(g_font, cx, cy, 0xFF80B3D6, 0.8f, "USERNAME");
    vita2d_draw_rectangle(cx, cy + 28, 300, 26, g_sel == 2 ? 0xFF005577 : 0xFF0A1218);
    vita2d_draw_rectangle(cx, cy + 28, 300, 26, g_sel == 2 ? 0xFF00AAFF : 0xFF3A4B5C);
    vita2d_pgf_draw_text(g_font, cx + 8, cy + 32, 0xFFFFFFFF, 0.8f, g_set_user);

    cy += 70;
    vita2d_pgf_draw_text(g_font, cx, cy, 0xFF80B3D6, 0.8f, "WORKER");
    vita2d_draw_rectangle(cx, cy + 28, 300, 26, g_sel == 3 ? 0xFF005577 : 0xFF0A1218);
    vita2d_draw_rectangle(cx, cy + 28, 300, 26, g_sel == 3 ? 0xFF00AAFF : 0xFF3A4B5C);
    vita2d_pgf_draw_text(g_font, cx + 8, cy + 32, 0xFFFFFFFF, 0.8f, g_set_worker);

    cy += 70;
    vita2d_pgf_draw_text(g_font, cx, cy, 0xFF80B3D6, 0.8f, "MINING KEY");
    vita2d_draw_rectangle(cx, cy + 28, 400, 26, g_sel == 4 ? 0xFF005577 : 0xFF0A1218);
    vita2d_draw_rectangle(cx, cy + 28, 400, 26, g_sel == 4 ? 0xFF00AAFF : 0xFF3A4B5C);
    vita2d_pgf_draw_text(g_font, cx + 8, cy + 32, 0xFFFFFFFF, 0.8f, g_set_key);

    cy += 70;
    vita2d_pgf_draw_text(g_font, cx, cy, 0xFF80B3D6, 0.8f, "DONATION ADDR");
    vita2d_draw_rectangle(cx, cy + 28, 400, 26, g_sel == 5 ? 0xFF005577 : 0xFF0A1218);
    vita2d_draw_rectangle(cx, cy + 28, 400, 26, g_sel == 5 ? 0xFF00AAFF : 0xFF3A4B5C);
    vita2d_pgf_draw_text(g_font, cx + 8, cy + 32, 0xFFFFFFFF, 0.8f, g_set_donation);

    cy += 70;
    vita2d_pgf_draw_text(g_font, cx, cy, 0xFF80B3D6, 0.8f, "AUTO-START");
    snprintf(hbuf, sizeof(hbuf), "%s", config_get_auto_start() ? "ON" : "OFF");
    vita2d_pgf_draw_text(g_font, cx, cy + 28, 0xFFC0C0C0, 0.8f, hbuf);
    Button btn_fetch = { 500, 380, 260, 64, "FETCH POOL" };
    Button btn_save = { 140, 460, 240, 64, "SAVE" };
    Button btn_back = { 420, 460, 240, 64, "BACK" };

    if (button_update(&btn_fetch, g_touch_pressed)) {
        ui_log("Fetching pool...");
        if (miner_fetch_pool() == 0) {
            ui_log("Pool: %s:%u", miner_pool_host(), miner_pool_port());
            snprintf(g_set_server, sizeof(g_set_server), "%s", miner_pool_host());
            snprintf(g_set_port, sizeof(g_set_port), "%u", miner_pool_port());
        } else {
            ui_log("Pool fetch failed");
        }
        g_button_cooldown = sceKernelGetSystemTimeWide();
    }
        if (button_update(&btn_save, g_touch_pressed)) {
            uint16_t p = (uint16_t)atoi(g_set_port);
            miner_set_server(g_set_server, p ? p : 12181);
            miner_set_username(g_set_user);
            miner_set_worker(g_set_worker);
            miner_set_mining_key(g_set_key);
            miner_set_auto_start(config_get_auto_start());

            g_cfg.auto_start = config_get_auto_start();
            snprintf(g_cfg.server, sizeof(g_cfg.server), "%s", g_set_server);
            g_cfg.port = p ? p : 12181;
            snprintf(g_cfg.username, sizeof(g_cfg.username), "%s", g_set_user);
            snprintf(g_cfg.worker, sizeof(g_cfg.worker), "%s", g_set_worker);
            snprintf(g_cfg.mining_key, sizeof(g_cfg.mining_key), "%s", g_set_key);
            snprintf(g_cfg.donation_addr, sizeof(g_cfg.donation_addr), "%s", g_set_donation);
            config_save(&g_cfg);
            ui_log("Settings saved");
            g_screen = SCREEN_DASHBOARD;
            g_button_cooldown = sceKernelGetSystemTimeWide();
        }
    if (button_update(&btn_back, g_touch_pressed)) {
        g_screen = SCREEN_DASHBOARD;
        g_button_cooldown = sceKernelGetSystemTimeWide();
    }

    draw_button(&btn_fetch);
    draw_button(&btn_save);
    draw_button(&btn_back);

    vita2d_end_drawing();
    vita2d_swap_buffers();
}

static void render_about(void) {
    vita2d_start_drawing();
    vita2d_draw_rectangle(0, 0, SCREEN_W, SCREEN_H, 0xFF0E1420);

    draw_panel(20, 72, 920, 388, "ABOUT");
    vita2d_pgf_draw_text(g_font, 80, 130, 0xFFFFFFFF, 1.2f, "VitaDucoMiner");
    vita2d_pgf_draw_text(g_font, 80, 170, 0xFFC0C0C0, 0.9f, "DuinoCoin Miner for PSVita");
    vita2d_pgf_draw_text(g_font, 80, 200, 0xFF80B3D6, 0.7f, "GUI: vita2d  SDL-free  VitaSDK");
    vita2d_pgf_draw_text(g_font, 80, 220, 0xFF80B3D6, 0.7f, "Algorithm: DUCO-S1 (SHA-1)");
    vita2d_pgf_draw_text(g_font, 80, 240, 0xFF808080, 0.7f, "Unofficial DuinoCoin miner");

    Button btn_back = { 360, 380, 240, 64, "BACK" };
    if (button_update(&btn_back, g_touch_pressed)) {
        g_screen = SCREEN_DASHBOARD;
        g_button_cooldown = sceKernelGetSystemTimeWide();
    }
    draw_button(&btn_back);

    vita2d_end_drawing();
    vita2d_swap_buffers();
}

static void render_donation(void) {
    vita2d_start_drawing();
    vita2d_draw_rectangle(0, 0, SCREEN_W, SCREEN_H, 0xFF0E1420);

    draw_panel(20, 72, 920, 388, "DONATE");
    vita2d_pgf_draw_text(g_font, 80, 130, 0xFFFFFFFF, 1.1f, "Support VitaDucoMiner");
    vita2d_pgf_draw_text(g_font, 80, 170, 0xFFC0C0C0, 0.8f, "DuinoCoin Address:");
    vita2d_pgf_draw_text(g_font, 80, 200, 0xFF00FF80, 0.9f, g_set_donation[0] ? g_set_donation : "(none set)");
    vita2d_pgf_draw_text(g_font, 80, 250, 0xFF808080, 0.7f, "Set donation address in Settings");

    Button btn_back = { 360, 380, 240, 64, "BACK" };
    if (button_update(&btn_back, g_touch_pressed)) {
        g_screen = SCREEN_DASHBOARD;
        g_button_cooldown = sceKernelGetSystemTimeWide();
    }
    draw_button(&btn_back);

    vita2d_end_drawing();
    vita2d_swap_buffers();
}

static void render_log(void) {
    vita2d_start_drawing();
    vita2d_draw_rectangle(0, 0, SCREEN_W, SCREEN_H, 0xFF0E1420);

    draw_panel(20, 72, 920, 388, "ACTIVITY LOG");
    int cnt = g_log_count;
    if (cnt == 0) {
        vita2d_pgf_draw_text(g_font, 80, 140, 0xFF808080, 0.8f, "(no log entries)");
    }
    for (int i = 0; i < cnt && i < 10; i++) {
        vita2d_pgf_draw_text(g_font, 80, 140 + i * 28, 0xFFC0C0C0, 0.7f, g_log_lines[i]);
    }

    Button btn_back = { 360, 380, 240, 64, "BACK" };
    if (button_update(&btn_back, g_touch_pressed)) {
        g_screen = SCREEN_DASHBOARD;
        g_button_cooldown = sceKernelGetSystemTimeWide();
    }
    draw_button(&btn_back);

    vita2d_end_drawing();
    vita2d_swap_buffers();
}

/* Config save helper declared in config.c */
void cfg_save_and_apply(const char *server, uint16_t port, const char *user,
                        const char *worker, const char *key, const char *donation, bool auto_start);

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    sceSysmoduleLoadModule(SCE_SYSMODULE_HTTP);

    Config cfg;
    if (config_load(&cfg) < 0) {
        config_set_defaults(&cfg);
    }
    memcpy(&g_cfg, &cfg, sizeof(Config));

    if (net_init() < 0) {
        ui_log("net_init failed");
    }

    if (vita2d_init() < 0) {
        net_close();
        sceKernelExitProcess(1);
    }

    g_font = vita2d_load_default_pgf();
    if (!g_font) {
        g_font = NULL;
    }

    pthread_t *miner_thread = miner_start();
    (void)miner_thread;

    miner_set_server(g_cfg.server, g_cfg.port);
    miner_set_username(g_cfg.username);
    miner_set_worker(g_cfg.worker);
    miner_set_mining_key(g_cfg.mining_key);
    miner_set_auto_start(g_cfg.auto_start);

    bool running = true;
    const char *ip = net_local_ip();
    snprintf(g_ip, sizeof(g_ip), "%s", ip ? ip : "0.0.0.0");

    g_last_stats = sceKernelGetSystemTimeWide();

    if (g_cfg.auto_start) {
        miner_toggle_mining();
        ui_log("Auto-start mining");
    }

    while (running) {
        SceCtrlData pad;
        sceCtrlPeekBufferPositive(0, &pad, 1);

        bool cross = btn_pressed(pad.buttons, g_prev_cross, SCE_CTRL_CROSS);
        bool circle = btn_pressed(pad.buttons, g_prev_circle, SCE_CTRL_CIRCLE);
        bool square = btn_pressed(pad.buttons, g_prev_square, SCE_CTRL_SQUARE);
        bool up = btn_pressed(pad.buttons, g_prev_up, SCE_CTRL_UP);
        bool down = btn_pressed(pad.buttons, g_prev_down, SCE_CTRL_DOWN);
        bool left = btn_pressed(pad.buttons, g_prev_left, SCE_CTRL_LEFT);
        bool right = btn_pressed(pad.buttons, g_prev_right, SCE_CTRL_RIGHT);

        g_prev_cross = pad.buttons & SCE_CTRL_CROSS;
        g_prev_circle = pad.buttons & SCE_CTRL_CIRCLE;
        g_prev_square = pad.buttons & SCE_CTRL_SQUARE;
        g_prev_up = pad.buttons & SCE_CTRL_UP;
        g_prev_down = pad.buttons & SCE_CTRL_DOWN;
        g_prev_left = pad.buttons & SCE_CTRL_LEFT;
        g_prev_right = pad.buttons & SCE_CTRL_RIGHT;

        update_touch();

        uint64_t now = sceKernelGetSystemTimeWide();
        bool cooldown_active = (now - g_button_cooldown) < 150000ull;

        Miner *m = miner_get_stats();
        if (!cooldown_active && now - g_last_stats >= 500000ull) {
            g_last_stats = now;
            g_cached_hashrate = m->hashrate;
            const char *ip2 = net_local_ip();
            snprintf(g_ip, sizeof(g_ip), "%s", ip2 ? ip2 : "0.0.0.0");
        }

        switch (g_screen) {
            case SCREEN_DASHBOARD:
                render_dashboard();
                if (!cooldown_active && square) {
                    g_screen = SCREEN_SETTINGS;
                    g_sel = 0;
                    g_button_cooldown = now;
                } else if (!cooldown_active && cross) {
                    /* handled by button */
                }
                break;

            case SCREEN_SETTINGS:
                if (!cooldown_active && circle) {
                    g_screen = SCREEN_DASHBOARD;
                    g_button_cooldown = now;
                } else if (!cooldown_active && up) {
                    g_sel = (g_sel > 0) ? g_sel - 1 : 6;
                    g_button_cooldown = now;
                } else if (!cooldown_active && down) {
                    g_sel = (g_sel < 6) ? g_sel + 1 : 0;
                    g_button_cooldown = now;
                }
                break;

            case SCREEN_SETTINGS_EDIT:
                /* handled in dashboard render via touch */
                break;

            case SCREEN_ABOUT:
                if (!cooldown_active && (circle || cross)) {
                    g_screen = SCREEN_DASHBOARD;
                    g_button_cooldown = now;
                }
                break;

            case SCREEN_DONATION:
                if (!cooldown_active && (circle || cross)) {
                    g_screen = SCREEN_DASHBOARD;
                    g_button_cooldown = now;
                }
                break;

            case SCREEN_LOG:
                if (!cooldown_active && (circle || cross)) {
                    g_screen = SCREEN_DASHBOARD;
                    g_button_cooldown = now;
                }
                break;
        }

        sceKernelDelayThread(16000);
    }

    miner_stop();
    miner_join();
    if (g_font) vita2d_free_pgf(g_font);
    vita2d_fini();
    net_close();
    sceKernelExitProcess(0);
    return 0;
}
