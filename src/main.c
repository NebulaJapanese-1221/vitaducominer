#include "miner.h"
#include "net.h"
#include "ui.h"
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/sysmodule.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int pt_in(int x, int y, int rx, int ry, int rw, int rh) {
    return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    /* Load required system modules before using their APIs */
    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    sceSysmoduleLoadModule(SCE_SYSMODULE_HTTP);

    if (net_init() < 0) {
        ui_log("net_init failed");
    }
    if (ui_init() < 0) {
        net_close();
        sceKernelExitProcess(1);
    }

    pthread_t *miner_thread = miner_start();
    (void)miner_thread;

    UiScreen screen = SCREEN_DASHBOARD;
    int active_field = -1;
    bool running = true;
    const char *ip = net_local_ip();

    uint64_t last_stats = sceKernelGetSystemTimeWide();
    double cached_hashrate = 0.0;
    char text_in[256] = {0};

    while (running) {
        bool quit = false;
        int tx = 0, ty = 0;
        bool pressed = false;
        ui_poll_events(&quit, &tx, &ty, &pressed, text_in, sizeof(text_in));
        if (quit) {
            running = false;
            break;
        }

        Miner *m = miner_get_stats();
        if (sceKernelGetSystemTimeWide() - last_stats >= 500000ull) {
            last_stats = sceKernelGetSystemTimeWide();
            cached_hashrate = m->hashrate;
            ip = net_local_ip();
        }

        UiStats us = {
            .connected = m->connected,
            .mining = m->mining,
            .ip = ip,
            .server = m->server,
            .port = m->port,
            .username = m->username,
            .mining_key = m->mining_key,
            .difficulty = m->difficulty,
            .hashes = m->hashes,
            .hashrate = cached_hashrate,
            .accepted = m->shares_accepted,
            .rejected = m->shares_rejected,
            .nonce = m->nonce,
            .uptime_ms = m->uptime_ms,
            .pool_host = miner_pool_host(),
            .pool_port = miner_pool_port(),
            .pool_valid = miner_pool_valid(),
        };

        UiAction action = ui_render(&us, screen, tx, ty, pressed, active_field);

        if (screen == SCREEN_SETTINGS) {
            if (text_in[0] && active_field >= 0) {
                ui_commit_active_field();
                ui_set_active_field(active_field);
            }
            if (pressed) {
                if (pt_in(tx, ty, 60, 140, 400, 26)) ui_set_active_field(0);
                else if (pt_in(tx, ty, 60, 220, 120, 26)) ui_set_active_field(1);
                else if (pt_in(tx, ty, 60, 300, 400, 26)) ui_set_active_field(2);
                else if (pt_in(tx, ty, 60, 380, 400, 26)) ui_set_active_field(3);
            }
        }

        switch (action) {
            case UI_ACTION_TOGGLE_MINING:
                miner_toggle_mining();
                ui_log("%s", miner_is_mining() ? "Mining started" : "Mining stopped");
                break;
            case UI_ACTION_OPEN_SETTINGS:
                screen = SCREEN_SETTINGS;
                active_field = -1;
                break;
            case UI_ACTION_OPEN_ABOUT:
                screen = SCREEN_ABOUT;
                active_field = -1;
                break;
            case UI_ACTION_BACK:
                screen = SCREEN_DASHBOARD;
                active_field = -1;
                break;
            case UI_ACTION_QUIT:
                running = false;
                break;
            case UI_ACTION_SAVE_SETTINGS: {
                ui_commit_active_field();
                uint16_t p = (uint16_t)atoi(ui_get_port());
                miner_set_server(ui_get_server(), p ? p : 12181);
                miner_set_username(ui_get_user());
                miner_set_mining_key(ui_get_key());
                ui_settings_clear_dirty();
                ui_log("Settings saved");
                screen = SCREEN_DASHBOARD;
                active_field = -1;
                break;
            }
            case UI_ACTION_FETCH_POOL: {
                ui_log("Fetching pool...");
                if (miner_fetch_pool() == 0) {
                    ui_log("Pool: %s:%u", miner_pool_host(), miner_pool_port());
                    ui_set_server_text(miner_pool_host());
                    {
                        char pbuf[8];
                        snprintf(pbuf, sizeof(pbuf), "%u", miner_pool_port());
                        ui_set_port_text(pbuf);
                    }
                } else {
                    ui_log("Pool fetch failed, using default");
                }
                screen = SCREEN_DASHBOARD;
                active_field = -1;
                break;
            }
            default: break;
        }

        sceKernelDelayThread(16000);
    }

    miner_stop();
    miner_join();
    ui_quit();
    net_close();
    sceKernelExitProcess(0);
    return 0;
}
