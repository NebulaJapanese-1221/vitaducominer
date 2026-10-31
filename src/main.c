#include "miner.h"
#include "net.h"
#include "ui.h"
#include "config.h"
#include "pool.h"
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/sysmodule.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int pt_in(int x, int y, int rx, int ry, int rw, int rh) {
    return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
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
        ui_log("net_init failed");
    }
    if (ui_init() < 0) {
        net_close();
        sceKernelExitProcess(1);
    }

    pthread_t *miner_thread = miner_start();
    (void)miner_thread;

    /* Apply loaded config to miner */
    miner_set_server(cfg.server, cfg.port);
    miner_set_username(cfg.username);
    miner_set_worker(cfg.worker);
    miner_set_mining_key(cfg.mining_key);
    miner_set_auto_start(cfg.auto_start);
    ui_set_auto_start(cfg.auto_start);

    UiScreen screen = SCREEN_DASHBOARD;
    int active_field = -1;
    bool running = true;
    const char *ip = net_local_ip();

    uint64_t last_stats = sceKernelGetSystemTimeWide();
    uint64_t last_hr_sample = sceKernelGetSystemTimeWide();
    double cached_hashrate = 0.0;
    double hr_history[HASHRATE_HISTORY_MAX];
    for (int i = 0; i < HASHRATE_HISTORY_MAX; i++) hr_history[i] = 0.0;
    int hr_idx = 0;

    PoolState pool_state;
    memset(&pool_state, 0, sizeof(pool_state));
    bool pool_fetch_pending = false;

    char text_in[256] = {0};

    /* Auto-start mining if configured */
    if (cfg.auto_start) {
        miner_toggle_mining();
        ui_log("Auto-start mining");
    }

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

        /* Sample hashrate for history graph once per second */
        if (sceKernelGetSystemTimeWide() - last_hr_sample >= 1000000ull) {
            last_hr_sample = sceKernelGetSystemTimeWide();
            hr_history[hr_idx] = cached_hashrate;
            hr_idx = (hr_idx + 1) % HASHRATE_HISTORY_MAX;
        }

        /* Check async pool fetch completion */
        if (pool_fetch_pending && !pool_state.fetching) {
            pool_fetch_join(&pool_state);
            if (pool_state.success) {
                ui_log("Pool: %s:%u", pool_state.pool.host, pool_state.pool.port);
                ui_set_server_text(pool_state.pool.host);
                {
                    char pbuf[8];
                    snprintf(pbuf, sizeof(pbuf), "%u", pool_state.pool.port);
                    ui_set_port_text(pbuf);
                }
                miner_set_server(pool_state.pool.host, pool_state.pool.port);
            } else {
                ui_log("Pool fetch failed, using default");
            }
            pool_fetch_pending = false;
        }

        UiStats us = {
            .connected = m->connected,
            .mining = m->mining,
            .auto_start = m->auto_start,
            .ip = ip,
            .server = m->server,
            .port = m->port,
            .username = m->username,
            .worker = m->worker,
            .mining_key = m->mining_key,
            .reject_reason = m->reject_reason,
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
            .pool_fetching = pool_fetch_pending,
            .donation_addr = cfg.donation_addr,
        };
        memcpy(us.hashrate_history, hr_history, sizeof(hr_history));

        UiAction action = ui_render(&us, screen, tx, ty, pressed, active_field);

        if (screen == SCREEN_SETTINGS) {
            if (text_in[0] && active_field >= 0) {
                ui_commit_active_field();
                ui_set_active_field(active_field);
            }
            if (pressed) {
                if (pt_in(tx, ty, 60, 128, 400, 26)) ui_set_active_field(0);
                else if (pt_in(tx, ty, 60, 208, 120, 26)) ui_set_active_field(1);
                else if (pt_in(tx, ty, 60, 288, 300, 26)) ui_set_active_field(2);
                else if (pt_in(tx, ty, 60, 368, 300, 26)) ui_set_active_field(3);
                else if (pt_in(tx, ty, 60, 448, 400, 26)) ui_set_active_field(4);
                else if (pt_in(tx, ty, 60, 528, 400, 26)) ui_set_active_field(5);
            }
        }

        switch (action) {
            case UI_ACTION_TOGGLE_MINING:
                miner_toggle_mining();
                ui_log("%s", miner_is_mining() ? "Mining started" : "Mining stopped");
                break;
            case UI_ACTION_TOGGLE_AUTO_START: {
                bool new_val = !m->auto_start;
                miner_set_auto_start(new_val);
                ui_set_auto_start(new_val);
                ui_log("Auto-start %s", new_val ? "enabled" : "disabled");
                break;
            }
            case UI_ACTION_OPEN_SETTINGS:
                screen = SCREEN_SETTINGS;
                active_field = -1;
                break;
            case UI_ACTION_OPEN_ABOUT:
                screen = SCREEN_ABOUT;
                active_field = -1;
                break;
            case UI_ACTION_OPEN_DONATION:
                screen = SCREEN_DONATION;
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
                miner_set_worker(ui_get_worker());
                miner_set_mining_key(ui_get_key());
                miner_set_auto_start(ui_get_auto_start());

                cfg.auto_start = ui_get_auto_start();
                snprintf(cfg.server, sizeof(cfg.server), "%s", ui_get_server());
                cfg.port = p ? p : 12181;
                snprintf(cfg.username, sizeof(cfg.username), "%s", ui_get_user());
                snprintf(cfg.worker, sizeof(cfg.worker), "%s", ui_get_worker());
                snprintf(cfg.mining_key, sizeof(cfg.mining_key), "%s", ui_get_key());
                snprintf(cfg.donation_addr, sizeof(cfg.donation_addr), "%s", ui_get_donation_addr());
                config_save(&cfg);
                ui_settings_clear_dirty();
                ui_log("Settings saved");
                screen = SCREEN_DASHBOARD;
                active_field = -1;
                break;
            }
            case UI_ACTION_FETCH_POOL: {
                if (!pool_fetch_pending) {
                    ui_log("Fetching pool...");
                    pool_fetch_async(&pool_state);
                    pool_fetch_pending = true;
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
