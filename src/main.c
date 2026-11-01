#include "miner.h"
#include "net.h"
#include "config.h"
#include "pool.h"
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/sysmodule.h>
#include <psp2/ctrl.h>
#include <psp2/display.h>
#include <vita2d.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define SCREEN_W 960
#define SCREEN_H 544

typedef enum {
    SCREEN_DASHBOARD,
    SCREEN_SETTINGS,
    SCREEN_SETTINGS_EDIT,
    SCREEN_ABOUT,
    SCREEN_LOG
} UiScreen;

static UiScreen g_screen = SCREEN_DASHBOARD;
static int g_sel = 0;
static int g_edit_field = 0;
static char g_text_in[256] = {0};

static bool g_prev_cross = false;
static bool g_prev_circle = false;
static bool g_prev_up = false;
static bool g_prev_down = false;
static bool g_prev_left = false;
static bool g_prev_right = false;

static uint64_t g_last_stats = 0;
static double g_cached_hashrate = 0.0;
static char g_ip[16] = {0};

static vita2d_pgf *g_font = NULL;

/* 5x7 bitmap font, one byte per row, bit 0x10 = leftmost pixel */
static const uint8_t font_bmp[95][7] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // space
    {0x0e,0x11,0x13,0x15,0x19,0x11,0x0e}, // !
    {0x04,0x0c,0x04,0x04,0x04,0x04,0x0e}, // "
    {0x0e,0x11,0x01,0x03,0x08,0x10,0x1f}, // #
    {0x0e,0x11,0x01,0x0e,0x01,0x11,0x0e}, // $
    {0x03,0x07,0x0b,0x19,0x11,0x1f,0x01}, // %
    {0x1f,0x10,0x1e,0x01,0x01,0x11,0x0e}, // &
    {0x0e,0x11,0x10,0x1e,0x11,0x11,0x0e}, // '
    {0x1f,0x01,0x02,0x04,0x08,0x08,0x08}, // (
    {0x0e,0x11,0x11,0x0e,0x11,0x11,0x0e}, // )
    {0x0e,0x11,0x11,0x0f,0x01,0x11,0x0e}, // *
    {0x0e,0x11,0x11,0x1f,0x11,0x11,0x11}, // +
    {0x1e,0x11,0x11,0x1e,0x11,0x11,0x1e}, // ,
    {0x0e,0x11,0x10,0x10,0x10,0x11,0x0e}, // -
    {0x1e,0x11,0x11,0x11,0x11,0x11,0x1e}, // .
    {0x1f,0x10,0x10,0x1e,0x10,0x10,0x1f}, // /
    {0x1f,0x10,0x10,0x1e,0x10,0x10,0x10}, // 0
    {0x0e,0x11,0x10,0x17,0x11,0x11,0x0e}, // 1
    {0x11,0x11,0x11,0x1f,0x11,0x11,0x11}, // 2
    {0x0e,0x04,0x04,0x04,0x04,0x11,0x0e}, // 3
    {0x1f,0x01,0x01,0x01,0x01,0x11,0x0e}, // 4
    {0x11,0x11,0x12,0x14,0x18,0x14,0x12}, // 5
    {0x10,0x10,0x10,0x10,0x10,0x11,0x0e}, // 6
    {0x11,0x1b,0x15,0x15,0x11,0x11,0x11}, // 7
    {0x11,0x13,0x17,0x1b,0x1d,0x11,0x11}, // 8
    {0x0e,0x11,0x11,0x11,0x11,0x11,0x0e}, // 9
    {0x1e,0x11,0x11,0x1e,0x10,0x10,0x10}, // :
    {0x0e,0x11,0x11,0x11,0x15,0x1a,0x0e}, // ;
    {0x1e,0x11,0x11,0x1e,0x14,0x12,0x11}, // <
    {0x0e,0x11,0x10,0x0e,0x01,0x11,0x0e}, // =
    {0x1f,0x04,0x04,0x04,0x04,0x04,0x04}, // >
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0e}, // ?
    {0x11,0x11,0x11,0x11,0x11,0x0a,0x04}, // @
    {0x11,0x11,0x11,0x15,0x15,0x1b,0x11}, // A
    {0x11,0x11,0x0a,0x04,0x0a,0x11,0x11}, // B
    {0x11,0x11,0x0a,0x04,0x04,0x04,0x04}, // C
    {0x1f,0x01,0x02,0x04,0x08,0x10,0x1f}, // D
    {0x00,0x00,0x10,0x00,0x10,0x00,0x00}, // E
    {0x00,0x00,0x00,0x00,0x00,0x10,0x10}, // F
    {0x00,0x00,0x10,0x08,0x04,0x02,0x01}, // G
    {0x00,0x00,0x00,0x10,0x00,0x00,0x00}, // H
    {0x0e,0x11,0x13,0x15,0x19,0x11,0x0e}, // I
    {0x04,0x0c,0x04,0x04,0x04,0x04,0x0e}, // J
    {0x0e,0x11,0x01,0x03,0x08,0x10,0x1f}, // K
    {0x0e,0x11,0x01,0x0e,0x01,0x11,0x0e}, // L
    {0x03,0x07,0x0b,0x19,0x11,0x1f,0x01}, // M
    {0x1f,0x10,0x1e,0x01,0x01,0x11,0x0e}, // N
    {0x0e,0x11,0x10,0x1e,0x11,0x11,0x0e}, // O
    {0x1f,0x01,0x02,0x04,0x08,0x08,0x08}, // P
    {0x0e,0x11,0x11,0x0e,0x11,0x11,0x0e}, // Q
    {0x0e,0x11,0x11,0x0f,0x01,0x11,0x0e}, // R
    {0x0e,0x11,0x11,0x1f,0x11,0x11,0x11}, // S
    {0x1e,0x11,0x11,0x1e,0x11,0x11,0x1e}, // T
    {0x0e,0x11,0x10,0x10,0x10,0x11,0x0e}, // U
    {0x1e,0x11,0x11,0x11,0x11,0x11,0x1e}, // V
    {0x1f,0x10,0x10,0x1e,0x10,0x10,0x1f}, // W
    {0x1f,0x10,0x10,0x1e,0x10,0x10,0x10}, // X
    {0x0e,0x11,0x10,0x17,0x11,0x11,0x0e}, // Y
    {0x11,0x11,0x11,0x1f,0x11,0x11,0x11}, // Z
    {0x0e,0x04,0x04,0x04,0x04,0x11,0x0e}, // [
    {0x1f,0x01,0x01,0x01,0x01,0x11,0x0e}, // \
    {0x11,0x11,0x12,0x14,0x18,0x14,0x12}, // ]
    {0x10,0x10,0x10,0x10,0x10,0x11,0x0e}, // ^
    {0x11,0x1b,0x15,0x15,0x11,0x11,0x11}, // _
    {0x11,0x13,0x17,0x1b,0x1d,0x11,0x11}, // `
    {0x0e,0x11,0x11,0x11,0x11,0x11,0x0e}, // a
    {0x1e,0x11,0x11,0x1e,0x10,0x10,0x10}, // b
    {0x0e,0x11,0x11,0x11,0x15,0x1a,0x0e}, // c
    {0x1e,0x11,0x11,0x1e,0x14,0x12,0x11}, // d
    {0x0e,0x11,0x10,0x0e,0x01,0x11,0x0e}, // e
    {0x1f,0x04,0x04,0x04,0x04,0x04,0x04}, // f
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0e}, // g
    {0x11,0x11,0x11,0x11,0x11,0x0a,0x04}, // h
    {0x11,0x11,0x11,0x15,0x15,0x1b,0x11}, // i
    {0x11,0x11,0x0a,0x04,0x0a,0x11,0x11}, // j
    {0x11,0x11,0x0a,0x04,0x04,0x04,0x04}, // k
    {0x1f,0x01,0x02,0x04,0x08,0x10,0x1f}, // l
    {0x00,0x00,0x10,0x00,0x10,0x00,0x00}, // m
    {0x00,0x00,0x00,0x00,0x00,0x10,0x10}, // n
    {0x00,0x00,0x10,0x08,0x04,0x02,0x01}, // o
    {0x00,0x00,0x00,0x10,0x00,0x00,0x00}  // p (placeholder)
};

static int font_char_index(char c) {
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
    return 0;
}

static void draw_bitmap_char(int x, int y, char c, unsigned int color) {
    int idx = font_char_index(c);
    if (idx >= 95) idx = 0;
    for (int row = 0; row < 7; row++) {
        uint8_t bits = font_bmp[idx][row];
        for (int col = 0; col < 5; col++) {
            if (bits & (0x10 >> col)) {
                vita2d_draw_pixel(x + col * 4, y + row * 4, color);
            }
        }
    }
}

static void draw_bitmap_text(int x, int y, const char *s, unsigned int color) {
    int cx = x;
    while (*s) {
        draw_bitmap_char(cx, y, *s, color);
        cx += 24;
        s++;
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
    } else {
        draw_bitmap_text(x, y, buf, color);
    }
}

static bool g_prev_square = false;
static bool g_button_cooldown = 0;

static int btn_pressed(unsigned int cur, unsigned int prev, unsigned int mask) {
    return (cur & mask) && !(prev & mask);
}

static void render_dashboard(void) {
    Miner *m = miner_get_stats();

    vita2d_start_drawing();
    vita2d_draw_rectangle(0, 0, SCREEN_W, SCREEN_H, 0xFF0E1420);

    draw_textf(20, 20, 0xFFFFFFFF, 1.0f, "=== VITA DUCO MINER ===");
    draw_textf(20, 50, 0xFF00FF00, 0.7f, "Status: %s", m->connected ? "ONLINE" : "OFFLINE");
    draw_textf(20, 75, 0xFFFFFFFF, 0.7f, "Mining: %s", m->mining ? "YES" : "NO");
    draw_textf(20, 100, 0xFFFFFFFF, 0.7f, "Auto-start: %s", config_get_auto_start() ? "ON" : "OFF");
    draw_textf(20, 125, 0xFF00FF00, 0.7f, "Hashrate: %.1f H/s", g_cached_hashrate);
    draw_textf(20, 150, 0xFFFFFFFF, 0.7f, "Hashes: %llu", (unsigned long long)m->hashes);
    draw_textf(20, 175, 0xFFFFFFFF, 0.7f, "Shares: A:%llu R:%llu",
        (unsigned long long)m->shares_accepted, (unsigned long long)m->shares_rejected);
    draw_textf(20, 200, 0xFFFFFFFF, 0.7f, "Difficulty: %u", m->difficulty);
    draw_textf(20, 225, 0xFFFFFFFF, 0.7f, "Nonce: %u", m->nonce);
    draw_textf(20, 250, 0xFFFFFFFF, 0.7f, "Uptime: %02llu:%02llu:%02llu",
        (unsigned long long)(m->uptime_ms / 3600000ull),
        (unsigned long long)((m->uptime_ms % 3600000ull) / 60000ull),
        (unsigned long long)((m->uptime_ms % 60000ull) / 1000ull));
    if (m->reject_reason[0]) {
        draw_textf(20, 275, 0xFFFF0000, 0.7f, "Last reject: %s", m->reject_reason);
    }
    draw_textf(20, 300, 0xFFFFFFFF, 0.7f, "Server: %s:%u", m->server, m->port);
    draw_textf(20, 325, 0xFFFFFFFF, 0.7f, "Username: %s%s", m->username,
        m->worker[0] ? "." : "", m->worker);
    draw_textf(20, 350, 0xFFFFFFFF, 0.7f, "IP: %s", g_ip[0] ? g_ip : "0.0.0.0");
    if (miner_pool_valid()) {
        draw_textf(20, 375, 0xFFFFFFFF, 0.7f, "Pool: %s:%u", miner_pool_host(), miner_pool_port());
    }

    draw_textf(20, 420, 0xFF808080, 0.7f, "[X] Start/Stop mining   [Square] Menu   [D-PAD] Navigate");

    vita2d_end_drawing();
    vita2d_swap_buffers();
}

static void render_settings(void) {
    vita2d_start_drawing();
    vita2d_draw_rectangle(0, 0, SCREEN_W, SCREEN_H, 0xFF0E1420);

    draw_textf(20, 20, 0xFFFFFFFF, 1.0f, "=== SETTINGS ===");
    draw_textf(40, 60, 0xFFFFFFFF, 0.7f, "[1] Server:      %s", g_set_server);
    draw_textf(40, 90, 0xFFFFFFFF, 0.7f, "[2] Port:        %s", g_set_port);
    draw_textf(40, 120, 0xFFFFFFFF, 0.7f, "[3] Username:    %s", g_set_user);
    draw_textf(40, 150, 0xFFFFFFFF, 0.7f, "[4] Worker:      %s", g_set_worker);
    draw_textf(40, 180, 0xFFFFFFFF, 0.7f, "[5] Mining key:  %s", g_set_key);
    draw_textf(40, 210, 0xFFFFFFFF, 0.7f, "[6] Donation:    %s", g_set_donation);
    draw_textf(40, 240, 0xFFFFFFFF, 0.7f, "[7] Auto-start:  %s", config_get_auto_start() ? "ON" : "OFF");
    draw_textf(40, 270, 0xFFFFFFFF, 0.7f, "[8] Fetch pool   (from getPool)");
    draw_textf(40, 300, 0xFFFFFFFF, 0.7f, "[9] Save         (to config.json)");

    draw_textf(20, 380, 0xFF808080, 0.7f, "[Circle] Back   [X] Select field   [D-PAD] Navigate");

    vita2d_end_drawing();
    vita2d_swap_buffers();
}

static void render_settings_edit(int field) {
    const char *labels[] = {"Server", "Port", "Username", "Worker", "Mining key", "Donation addr"};
    const char *cur = config_field_buf(field);

    vita2d_start_drawing();
    vita2d_draw_rectangle(0, 0, SCREEN_W, SCREEN_H, 0xFF0E1420);

    draw_textf(20, 20, 0xFFFFFFFF, 1.0f, "=== EDIT %s ===", labels[field]);
    draw_textf(40, 60, 0xFFFFFFFF, 0.7f, "Current: %s", cur);
    draw_textf(40, 90, 0xFFFFFFFF, 0.7f, "Editing: %s_", g_text_in);

    draw_textf(20, 200, 0xFF808080, 0.6f, "[Left/Right] Move cursor");
    draw_textf(20, 220, 0xFF808080, 0.6f, "[Up/Down]    Change char");
    draw_textf(20, 240, 0xFF808080, 0.6f, "[X] Save   [Circle] Cancel");

    vita2d_end_drawing();
    vita2d_swap_buffers();
}

static void render_about(void) {
    vita2d_start_drawing();
    vita2d_draw_rectangle(0, 0, SCREEN_W, SCREEN_H, 0xFF0E1420);

    draw_textf(20, 40, 0xFFFFFFFF, 1.0f, "VitaDucoMiner");
    draw_textf(20, 80, 0xFF00FF00, 0.7f, "DuinoCoin Miner for PSVita");
    draw_textf(20, 100, 0xFF808080, 0.6f, "UI: Text mode (dpad + X/O)");
    draw_textf(20, 120, 0xFF808080, 0.6f, "Algorithm: DUCO-S1 (SHA-1)");
    draw_textf(20, 140, 0xFF808080, 0.6f, "Unofficial DuinoCoin miner");

    draw_textf(20, 260, 0xFF808080, 0.7f, "[Circle/X] Back");

    vita2d_end_drawing();
    vita2d_swap_buffers();
}

static void render_log(void) {
    vita2d_start_drawing();
    vita2d_draw_rectangle(0, 0, SCREEN_W, SCREEN_H, 0xFF0E1420);

    draw_textf(20, 20, 0xFFFFFFFF, 1.0f, "=== ACTIVITY LOG ===");
    int cnt = config_log_count();
    if (cnt == 0) {
        draw_textf(20, 60, 0xFF808080, 0.7f, "(no log entries)");
    }
    for (int i = 0; i < cnt && i < 10; i++) {
        draw_textf(20, 60 + i * 25, 0xFFC0C0C0, 0.6f, "%s", config_log_line(i));
    }

    draw_textf(20, 360, 0xFF808080, 0.7f, "[Circle/X] Back");

    vita2d_end_drawing();
    vita2d_swap_buffers();
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    sceSysmoduleLoadModule(SCE_SYSMODULE_HTTP);

    Config cfg;
    if (config_load(&cfg) < 0) {
        config_set_defaults(&cfg);
    }

    if (net_init() < 0) {
        config_log("net_init failed");
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

    miner_set_server(cfg.server, cfg.port);
    miner_set_username(cfg.username);
    miner_set_worker(cfg.worker);
    miner_set_mining_key(cfg.mining_key);
    miner_set_auto_start(cfg.auto_start);

    bool running = true;
    const char *ip = net_local_ip();
    snprintf(g_ip, sizeof(g_ip), "%s", ip ? ip : "0.0.0.0");

    g_last_stats = sceKernelGetSystemTimeWide();

    if (cfg.auto_start) {
        miner_toggle_mining();
        config_log("Auto-start mining");
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

        /* Button cooldown to prevent flickering when holding buttons */
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
                if (!cooldown_active && cross) {
                    miner_toggle_mining();
                    config_log("%s", miner_get_stats()->mining ? "Mining started" : "Mining stopped");
                    g_button_cooldown = now;
                } else if (!cooldown_active && square) {
                    g_screen = SCREEN_SETTINGS;
                    g_sel = 0;
                    g_button_cooldown = now;
                }
                break;

            case SCREEN_SETTINGS:
                render_settings();
                if (!cooldown_active && circle) {
                    g_screen = SCREEN_DASHBOARD;
                    g_button_cooldown = now;
                } else if (!cooldown_active && cross) {
                    if (g_sel == 7) {
                        config_log("Fetching pool...");
                        if (miner_fetch_pool() == 0) {
                            config_log("Pool: %s:%u", miner_pool_host(), miner_pool_port());
                            snprintf(g_set_server, sizeof(g_set_server), "%s", miner_pool_host());
                            char pbuf[8];
                            snprintf(pbuf, sizeof(pbuf), "%u", miner_pool_port());
                            snprintf(g_set_port, sizeof(g_set_port), "%s", pbuf);
                        } else {
                            config_log("Pool fetch failed");
                        }
                        g_button_cooldown = now;
                    } else if (g_sel == 8) {
                        config_commit_field(0, g_set_server);
                        config_commit_field(1, g_set_port);
                        config_commit_field(2, g_set_user);
                        config_commit_field(3, g_set_worker);
                        config_commit_field(4, g_set_key);
                        config_commit_field(5, g_set_donation);

                        uint16_t p = (uint16_t)atoi(g_set_port);
                        miner_set_server(g_set_server, p ? p : 12181);
                        miner_set_username(g_set_user);
                        miner_set_worker(g_set_worker);
                        miner_set_mining_key(g_set_key);
                        miner_set_auto_start(config_get_auto_start());

                        cfg.auto_start = config_get_auto_start();
                        snprintf(cfg.server, sizeof(cfg.server), "%s", g_set_server);
                        cfg.port = p ? p : 12181;
                        snprintf(cfg.username, sizeof(cfg.username), "%s", g_set_user);
                        snprintf(cfg.worker, sizeof(cfg.worker), "%s", g_set_worker);
                        snprintf(cfg.mining_key, sizeof(cfg.mining_key), "%s", g_set_key);
                        snprintf(cfg.donation_addr, sizeof(cfg.donation_addr), "%s", g_set_donation);
                        config_save(&cfg);
                        config_log("Settings saved");
                        g_button_cooldown = now;
                    } else {
                        g_screen = SCREEN_SETTINGS_EDIT;
                        g_edit_field = g_sel;
                        snprintf(g_text_in, sizeof(g_text_in), "%s", config_field_buf(g_sel));
                        g_button_cooldown = now;
                    }
                } else if (!cooldown_active && up) {
                    g_sel = (g_sel > 0) ? g_sel - 1 : 8;
                    g_button_cooldown = now;
                } else if (!cooldown_active && down) {
                    g_sel = (g_sel < 8) ? g_sel + 1 : 0;
                    g_button_cooldown = now;
                }
                break;

            case SCREEN_SETTINGS_EDIT:
                render_settings_edit(g_edit_field);
                if (!cooldown_active && circle) {
                    g_screen = SCREEN_SETTINGS;
                    g_button_cooldown = now;
                } else if (!cooldown_active && cross) {
                    config_commit_field(g_edit_field, g_text_in);
                    g_screen = SCREEN_SETTINGS;
                    g_button_cooldown = now;
                } else if (!cooldown_active && left) {
                    int len = (int)strlen(g_text_in);
                    if (len > 0) g_text_in[len - 1] = 0;
                    g_button_cooldown = now;
                } else if (!cooldown_active && right) {
                    const char *cur = config_field_buf(g_edit_field);
                    int len = (int)strlen(g_text_in);
                    if (cur[len] && len < (int)sizeof(g_text_in) - 1) {
                        g_text_in[len] = cur[len];
                        g_text_in[len + 1] = 0;
                    }
                    g_button_cooldown = now;
                } else if (!cooldown_active && up) {
                    int len = (int)strlen(g_text_in);
                    if (len < (int)sizeof(g_text_in) - 1) {
                        g_text_in[len] = 'A';
                        g_text_in[len + 1] = 0;
                    }
                    g_button_cooldown = now;
                } else if (!cooldown_active && down) {
                    int len = (int)strlen(g_text_in);
                    if (len > 0) g_text_in[len - 1] = 0;
                    g_button_cooldown = now;
                }
                break;

            case SCREEN_ABOUT:
                render_about();
                if (!cooldown_active && (circle || cross)) {
                    g_screen = SCREEN_DASHBOARD;
                    g_button_cooldown = now;
                }
                break;

            case SCREEN_LOG:
                render_log();
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
