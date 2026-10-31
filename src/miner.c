#include "miner.h"
#include "net.h"
#include "duco-s1.h"
#include "pool.h"
#include "config.h"
#include <psp2/kernel/threadmgr.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define MAX_ITER 200000u
#define BACKOFF_BASE_MS 1000ull
#define BACKOFF_MAX_MS 30000ull

static Miner g_miner;
static pthread_t g_miner_thread;
static bool g_thread_started = false;
static pthread_mutex_t g_miner_lock = PTHREAD_MUTEX_INITIALIZER;

static uint64_t get_time_ms(void) {
    uint64_t t = sceKernelGetSystemTimeWide();
    return t / 1000ull;
}

static void set_reject_reason(Miner *m, const char *reason) {
    snprintf(m->reject_reason, sizeof(m->reject_reason), "%s", reason);
}

static void handle_job(Miner *m, const char *job) {
    m->difficulty = 1;
    const char *d = strchr(job, ':');
    if (d) {
        m->difficulty = (uint32_t)strtoul(d + 1, NULL, 10);
        if (m->difficulty == 0) {
            m->difficulty = 1;
        }
    }
}

static void build_worker_string(char *out, int out_size, const Miner *m) {
    if (m->worker[0]) {
        snprintf(out, out_size, "%s.%s", m->username, m->worker);
    } else {
        snprintf(out, out_size, "%s", m->username);
    }
}

static void *miner_loop(void *arg) {
    (void)arg;

    while (g_miner.running) {
        if (!g_miner.mining) {
            pthread_mutex_lock(&g_miner_lock);
            g_miner.connected = false;
            g_miner.start_time_ms = 0;
            g_miner.reconnect_backoff_ms = BACKOFF_BASE_MS;
            pthread_mutex_unlock(&g_miner_lock);
            sceKernelDelayThread(100000);
            continue;
        }

        pthread_mutex_lock(&g_miner_lock);
        const char *server = g_miner.server;
        uint16_t port = g_miner.port;
        const char *mining_key = g_miner.mining_key;
        pthread_mutex_unlock(&g_miner_lock);

        char worker_str[128];
        build_worker_string(worker_str, sizeof(worker_str), &g_miner);

        if (net_connect(server, port) < 0) {
            pthread_mutex_lock(&g_miner_lock);
            g_miner.connected = false;
            set_reject_reason(&g_miner, "connect failed");
            if (g_miner.reconnect_backoff_ms < BACKOFF_MAX_MS) {
                g_miner.reconnect_backoff_ms *= 2;
            }
            pthread_mutex_unlock(&g_miner_lock);
            sceKernelDelayThread((int)g_miner.reconnect_backoff_ms * 1000);
            continue;
        }

        pthread_mutex_lock(&g_miner_lock);
        g_miner.connected = true;
        g_miner.start_time_ms = get_time_ms();
        g_miner.reconnect_backoff_ms = BACKOFF_BASE_MS;
        pthread_mutex_unlock(&g_miner_lock);

        uint8_t buf[512];
        int rd = net_recv(buf, sizeof(buf) - 1);
        if (rd > 0) {
            buf[rd] = 0;
            handle_job(&g_miner, (const char *)buf);
        }

        uint8_t msg[256];
        int msglen;
        if (mining_key[0]) {
            msglen = snprintf((char *)msg, sizeof(msg), "%s:%s\n", worker_str, mining_key);
        } else {
            msglen = snprintf((char *)msg, sizeof(msg), "%s\n", worker_str);
        }
        net_send(msg, msglen);

        uint64_t last_hash_time = get_time_ms();
        uint64_t hash_count = 0;

        while (g_miner.running && g_miner.mining) {
            uint32_t nonce = 0;
            if (!duco_s1_mine(server, g_miner.difficulty, &nonce, g_miner.last_digest, MAX_ITER)) {
                continue;
            }

            uint64_t now = get_time_ms();
            hash_count += MAX_ITER;
            g_miner.hashes += MAX_ITER;
            g_miner.nonce = nonce;

            uint8_t out[256];
            int olen;
            if (mining_key[0]) {
                olen = snprintf((char *)out, sizeof(out), "%s:%u:%s\n", worker_str, nonce, mining_key);
            } else {
                olen = snprintf((char *)out, sizeof(out), "%s:%u:%s\n", worker_str, nonce, worker_str);
            }
            net_send(out, olen);

            rd = net_recv(buf, sizeof(buf) - 1);
            if (rd <= 0) {
                set_reject_reason(&g_miner, "connection lost");
                break;
            }
            buf[rd] = 0;
            const char *resp = (const char *)buf;
            if (strstr(resp, "ACCEPT") || strstr(resp, "OK")) {
                g_miner.shares_accepted++;
                set_reject_reason(&g_miner, "");
            } else if (strstr(resp, "REJECT")) {
                g_miner.shares_rejected++;
                if (strstr(resp, "difficulty")) {
                    set_reject_reason(&g_miner, "wrong difficulty");
                } else if (strstr(resp, "stale")) {
                    set_reject_reason(&g_miner, "stale job");
                } else if (strstr(resp, "nonce")) {
                    set_reject_reason(&g_miner, "bad nonce");
                } else {
                    set_reject_reason(&g_miner, "rejected by server");
                }
            } else if (strstr(resp, "JOB")) {
                handle_job(&g_miner, resp);
            }

            if (now - last_hash_time >= 1000ull) {
                double dt = (double)(now - last_hash_time) / 1000.0;
                pthread_mutex_lock(&g_miner_lock);
                g_miner.hashrate = (double)hash_count / dt;
                g_miner.uptime_ms = now - g_miner.start_time_ms;
                pthread_mutex_unlock(&g_miner_lock);
                hash_count = 0;
                last_hash_time = now;
            }

            sceKernelDelayThread(0);
        }

        net_close();
        pthread_mutex_lock(&g_miner_lock);
        g_miner.connected = false;
        g_miner.hashrate = 0.0;
        pthread_mutex_unlock(&g_miner_lock);
    }

    return NULL;
}

pthread_t *miner_start(void) {
    memset(&g_miner, 0, sizeof(g_miner));
    snprintf(g_miner.username, sizeof(g_miner.username), "vita_miner");
    g_miner.worker[0] = 0;
    snprintf(g_miner.server, sizeof(g_miner.server), "server.duinocoin.com");
    g_miner.port = 12181;
    g_miner.difficulty = 1;
    g_miner.running = true;
    g_miner.mining = false;
    g_miner.auto_start = false;
    g_miner.pool_fetched = false;
    g_miner.reconnect_backoff_ms = BACKOFF_BASE_MS;
    g_thread_started = true;
    pthread_create(&g_miner_thread, NULL, miner_loop, NULL);
    return &g_miner_thread;
}

void miner_stop(void) {
    g_miner.mining = false;
}

void miner_join(void) {
    if (g_thread_started) {
        g_miner.running = false;
        pthread_join(g_miner_thread, NULL);
        g_thread_started = false;
    }
    net_close();
}

Miner *miner_get_stats(void) {
    return &g_miner;
}

void miner_set_server(const char *server, uint16_t port) {
    pthread_mutex_lock(&g_miner_lock);
    snprintf(g_miner.server, sizeof(g_miner.server), "%s", server);
    g_miner.port = port;
    pthread_mutex_unlock(&g_miner_lock);
}

void miner_set_username(const char *username) {
    pthread_mutex_lock(&g_miner_lock);
    snprintf(g_miner.username, sizeof(g_miner.username), "%s", username);
    pthread_mutex_unlock(&g_miner_lock);
}

void miner_set_worker(const char *worker) {
    pthread_mutex_lock(&g_miner_lock);
    snprintf(g_miner.worker, sizeof(g_miner.worker), "%s", worker);
    pthread_mutex_unlock(&g_miner_lock);
}

void miner_set_mining_key(const char *key) {
    pthread_mutex_lock(&g_miner_lock);
    snprintf(g_miner.mining_key, sizeof(g_miner.mining_key), "%s", key);
    pthread_mutex_unlock(&g_miner_lock);
}

void miner_set_auto_start(bool enabled) {
    pthread_mutex_lock(&g_miner_lock);
    g_miner.auto_start = enabled;
    pthread_mutex_unlock(&g_miner_lock);
}

bool miner_get_auto_start(void) {
    bool a;
    pthread_mutex_lock(&g_miner_lock);
    a = g_miner.auto_start;
    pthread_mutex_unlock(&g_miner_lock);
    return a;
}

bool miner_is_mining(void) {
    bool m;
    pthread_mutex_lock(&g_miner_lock);
    m = g_miner.mining;
    pthread_mutex_unlock(&g_miner_lock);
    return m;
}

void miner_toggle_mining(void) {
    pthread_mutex_lock(&g_miner_lock);
    g_miner.mining = !g_miner.mining;
    if (g_miner.mining && g_miner.start_time_ms == 0) {
        g_miner.start_time_ms = get_time_ms();
    }
    pthread_mutex_unlock(&g_miner_lock);
}

int miner_fetch_pool(void) {
    PoolInfo pool;
    int ret = pool_fetch_sync(&pool);
    if (ret < 0) {
        pthread_mutex_lock(&g_miner_lock);
        g_miner.pool_fetched = false;
        pthread_mutex_unlock(&g_miner_lock);
        return -1;
    }
    pthread_mutex_lock(&g_miner_lock);
    snprintf(g_miner.server, sizeof(g_miner.server), "%s", pool.host);
    g_miner.port = pool.port;
    g_miner.pool_fetched = true;
    pthread_mutex_unlock(&g_miner_lock);
    return 0;
}

const char *miner_pool_host(void) {
    return g_miner.server;
}

uint16_t miner_pool_port(void) {
    return g_miner.port;
}

bool miner_pool_valid(void) {
    return g_miner.pool_fetched;
}
