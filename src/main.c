#include "miner.h"
#include "net.h"
#include "config.h"
#include "pool.h"
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/sysmodule.h>
#include <psp2/ctrl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    SCREEN_DASHBOARD,
    SCREEN_SETTINGS,
    SCREEN_SETTINGS_EDIT,
    SCREEN_ABOUT,
    SCREEN_LOG
} UiScreen;

#define SETTINGS_FIELD_COUNT 9

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

static int btn_pressed(unsigned int cur, unsigned int prev, unsigned int mask) {
    return (cur & mask) && !(prev & mask);
}

static void render_dashboard(void) {
    Miner *m = miner_get_stats();

    printf("\033[2J\033[H");
    printf("=== VITA DUCO MINER ===\n");
    printf("Status: %s\n", m->connected ? "ONLINE" : "OFFLINE");
    printf("Mining: %s\n", m->mining ? "YES" : "NO");
    printf("Auto-start: %s\n", config_get_auto_start() ? "ON" : "OFF");
    printf("Hashrate: %.1f H/s\n", g_cached_hashrate);
    printf("Hashes: %llu\n", (unsigned long long)m->hashes);
    printf("Shares: A:%llu R:%llu\n", (unsigned long long)m->shares_accepted,
              (unsigned long long)m->shares_rejected);
    printf("Difficulty: %u\n", m->difficulty);
    printf("Nonce: %u\n", m->nonce);
    printf("Uptime: %02llu:%02llu:%02llu\n",
        (unsigned long long)(m->uptime_ms / 3600000ull),
        (unsigned long long)((m->uptime_ms % 3600000ull) / 60000ull),
        (unsigned long long)((m->uptime_ms % 60000ull) / 1000ull));
    if (m->reject_reason[0]) {
        printf("Last reject: %s\n", m->reject_reason);
    }
    printf("Server: %s:%u\n", m->server, m->port);
    printf("Username: %s", m->username);
    if (m->worker[0]) printf(".%s", m->worker);
    printf("\n");
    printf("IP: %s\n", g_ip[0] ? g_ip : "0.0.0.0");
    if (miner_pool_valid()) {
        printf("Pool: %s:%u\n", miner_pool_host(), miner_pool_port());
    }
    printf("\n");
    printf("[X] Start/Stop mining\n");
    printf("[O] Menu\n");
    printf("[D-PAD] Navigate\n");
    fflush(stdout);
}

static void render_settings(void) {
    printf("\033[2J\033[H");
    printf("=== SETTINGS ===\n");
    printf("  [1] Server:      %s\n", g_set_server);
    printf("  [2] Port:        %s\n", g_set_port);
    printf("  [3] Username:    %s\n", g_set_user);
    printf("  [4] Worker:      %s\n", g_set_worker);
    printf("  [5] Mining key:  %s\n", g_set_key);
    printf("  [6] Donation:    %s\n", g_set_donation);
    printf("  [7] Auto-start:  %s\n", config_get_auto_start() ? "ON" : "OFF");
    printf("  [8] Fetch pool   (from getPool)\n");
    printf("  [9] Save         (to config.json)\n");
    printf("\n");
    printf("[O] Back\n");
    printf("[X] Select field\n");
    printf("[D-PAD] Navigate\n");
    fflush(stdout);
}

static void render_settings_edit(int field) {
    const char *labels[] = {"Server", "Port", "Username", "Worker", "Mining key", "Donation addr"};
    const char *cur = config_field_buf(field);
    printf("\033[2J\033[H");
    printf("=== EDIT %s ===\n", labels[field]);
    printf("Current: %s\n", cur);
    printf("Editing: %s_\n", g_text_in);
    printf("\n");
    printf("[Left/Right] Move cursor\n");
    printf("[Up/Down]    Change char\n");
    printf("[X]          Save\n");
    printf("[O]          Cancel\n");
    fflush(stdout);
}

static void render_about(void) {
    printf("\033[2J\033[H");
    printf("=== ABOUT ===\n");
    printf("VitaDucoMiner\n");
    printf("DuinoCoin Miner for PSVita\n");
    printf("UI: Text mode (dpad + X/O)\n");
    printf("Algorithm: DUCO-S1 (SHA-1)\n");
    printf("Unofficial DuinoCoin miner\n");
    printf("\n");
    printf("[O/X] Back\n");
    fflush(stdout);
}

static void render_log(void) {
    printf("\033[2J\033[H");
    printf("=== ACTIVITY LOG ===\n");
    int cnt = config_log_count();
    if (cnt == 0) {
        printf("(no log entries)\n");
    }
    for (int i = 0; i < cnt; i++) {
        printf("%s\n", config_log_line(i));
    }
    printf("\n");
    printf("[O/X] Back\n");
    fflush(stdout);
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
        bool up = btn_pressed(pad.buttons, g_prev_up, SCE_CTRL_UP);
        bool down = btn_pressed(pad.buttons, g_prev_down, SCE_CTRL_DOWN);
        bool left = btn_pressed(pad.buttons, g_prev_left, SCE_CTRL_LEFT);
        bool right = btn_pressed(pad.buttons, g_prev_right, SCE_CTRL_RIGHT);

        g_prev_cross = pad.buttons & SCE_CTRL_CROSS;
        g_prev_circle = pad.buttons & SCE_CTRL_CIRCLE;
        g_prev_up = pad.buttons & SCE_CTRL_UP;
        g_prev_down = pad.buttons & SCE_CTRL_DOWN;
        g_prev_left = pad.buttons & SCE_CTRL_LEFT;
        g_prev_right = pad.buttons & SCE_CTRL_RIGHT;

        Miner *m = miner_get_stats();
        if (sceKernelGetSystemTimeWide() - g_last_stats >= 500000ull) {
            g_last_stats = sceKernelGetSystemTimeWide();
            g_cached_hashrate = m->hashrate;
            const char *ip2 = net_local_ip();
            snprintf(g_ip, sizeof(g_ip), "%s", ip2 ? ip2 : "0.0.0.0");
        }

        switch (g_screen) {
            case SCREEN_DASHBOARD:
                render_dashboard();
                if (cross) {
                    miner_toggle_mining();
                    config_log("%s", miner_get_stats()->mining ? "Mining started" : "Mining stopped");
                } else if (circle) {
                    g_screen = SCREEN_SETTINGS;
                    g_sel = 0;
                }
                break;

            case SCREEN_SETTINGS:
                render_settings();
                if (circle) {
                    g_screen = SCREEN_DASHBOARD;
                } else if (cross) {
                    if (g_sel == 7) {
                        /* Fetch pool */
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
                    } else if (g_sel == 8) {
                        /* Save */
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
                    } else {
                        g_screen = SCREEN_SETTINGS_EDIT;
                        g_edit_field = g_sel;
                        snprintf(g_text_in, sizeof(g_text_in), "%s", config_field_buf(g_sel));
                    }
                } else if (up) {
                    g_sel = (g_sel > 0) ? g_sel - 1 : SETTINGS_FIELD_COUNT - 1;
                } else if (down) {
                    g_sel = (g_sel < SETTINGS_FIELD_COUNT - 1) ? g_sel + 1 : 0;
                }
                break;

            case SCREEN_SETTINGS_EDIT:
                render_settings_edit(g_edit_field);
                if (circle) {
                    g_screen = SCREEN_SETTINGS;
                } else if (cross) {
                    config_commit_field(g_edit_field, g_text_in);
                    g_screen = SCREEN_SETTINGS;
                } else if (left) {
                    int len = (int)strlen(g_text_in);
                    if (len > 0) g_text_in[len - 1] = 0;
                } else if (right) {
                    /* append next char from current field value as default */
                    const char *cur = config_field_buf(g_edit_field);
                    int len = (int)strlen(g_text_in);
                    if (cur[len] && len < (int)sizeof(g_text_in) - 1) {
                        g_text_in[len] = cur[len];
                        g_text_in[len + 1] = 0;
                    }
                } else if (up) {
                    int len = (int)strlen(g_text_in);
                    if (len < (int)sizeof(g_text_in) - 1) {
                        g_text_in[len] = 'A';
                        g_text_in[len + 1] = 0;
                    }
                } else if (down) {
                    int len = (int)strlen(g_text_in);
                    if (len > 0) g_text_in[len - 1] = 0;
                }
                break;

            case SCREEN_ABOUT:
                render_about();
                if (circle || cross) {
                    g_screen = SCREEN_DASHBOARD;
                }
                break;

            case SCREEN_LOG:
                render_log();
                if (circle || cross) {
                    g_screen = SCREEN_DASHBOARD;
                }
                break;
        }

        sceKernelDelayThread(50000);
    }

    miner_stop();
    miner_join();
    printf("VitaDucoMiner exiting...\n");
    fflush(stdout);
    sceKernelDelayThread(500000);
    net_close();
    sceKernelExitProcess(0);
    return 0;
}
