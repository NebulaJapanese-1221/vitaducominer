#include "config.h"
#include "json.h"
#include <psp2/io/fcntl.h>
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/threadmgr.h>
#include <pthread.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define CONFIG_PATH "ux0:/vitaducominer/config.json"

char g_set_server[128] = "server.duinocoin.com";
char g_set_port[8] = "12181";
char g_set_user[64] = "vita_miner";
char g_set_worker[64] = "";
char g_set_key[64] = "";
char g_set_donation[64] = "";
bool g_set_auto_start = false;

static char g_log_lines[8][96];
static int g_log_count = 0;
static pthread_mutex_t g_log_lock = PTHREAD_MUTEX_INITIALIZER;

static int ensure_dir(const char *path) {
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "%s", path);
    char *p = tmp + 1;
    while ((p = strchr(p, '/')) != NULL) {
        *p = 0;
        sceIoMkdir(tmp, 0777);
        *p = '/';
        p++;
    }
    return 0;
}

static int read_file(const char *path, char *buf, int buf_size) {
    int fd = sceIoOpen(path, SCE_O_RDONLY, 0);
    if (fd < 0) return -1;
    int total = 0;
    while (total < buf_size - 1) {
        int rd = sceIoRead(fd, buf + total, buf_size - 1 - total);
        if (rd <= 0) break;
        total += rd;
    }
    buf[total] = 0;
    sceIoClose(fd);
    return total;
}

static int write_file(const char *path, const char *buf) {
    ensure_dir(path);
    int fd = sceIoOpen(path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    if (fd < 0) return -1;
    int len = (int)strlen(buf);
    int written = sceIoWrite(fd, buf, len);
    sceIoClose(fd);
    return written == len ? 0 : -1;
}

void config_set_defaults(Config *c) {
    memset(c, 0, sizeof(*c));
    snprintf(c->server, sizeof(c->server), "server.duinocoin.com");
    c->port = 12181;
    snprintf(c->username, sizeof(c->username), "vita_miner");
    c->worker[0] = 0;
    c->mining_key[0] = 0;
    c->donation_addr[0] = 0;
    c->auto_start = false;
    c->pool_fetched = false;
    c->pool_host[0] = 0;
    c->pool_port = 0;
}

int config_load(Config *c) {
    config_set_defaults(c);
    char buf[2048];
    int len = read_file(CONFIG_PATH, buf, sizeof(buf));
    if (len <= 0) return -1;

    char tmp[256];
    if (json_get_string(buf, "server", tmp, sizeof(tmp))) {
        snprintf(c->server, sizeof(c->server), "%s", tmp);
        snprintf(g_set_server, sizeof(g_set_server), "%s", tmp);
    }
    long port = 0;
    if (json_get_number(buf, "port", &port) && port > 0) {
        c->port = (uint16_t)port;
        snprintf(g_set_port, sizeof(g_set_port), "%u", c->port);
    }
    if (json_get_string(buf, "username", tmp, sizeof(tmp))) {
        snprintf(c->username, sizeof(c->username), "%s", tmp);
        snprintf(g_set_user, sizeof(g_set_user), "%s", tmp);
    }
    if (json_get_string(buf, "worker", tmp, sizeof(tmp))) {
        snprintf(c->worker, sizeof(c->worker), "%s", tmp);
        snprintf(g_set_worker, sizeof(g_set_worker), "%s", tmp);
    }
    if (json_get_string(buf, "mining_key", tmp, sizeof(tmp))) {
        snprintf(c->mining_key, sizeof(c->mining_key), "%s", tmp);
        snprintf(g_set_key, sizeof(g_set_key), "%s", tmp);
    }
    if (json_get_string(buf, "donation_addr", tmp, sizeof(tmp))) {
        snprintf(c->donation_addr, sizeof(c->donation_addr), "%s", tmp);
        snprintf(g_set_donation, sizeof(g_set_donation), "%s", tmp);
    }
    long auto_start = 0;
    if (json_get_number(buf, "auto_start", &auto_start)) {
        c->auto_start = (auto_start != 0);
        g_set_auto_start = c->auto_start;
    }
    if (json_get_string(buf, "pool_host", tmp, sizeof(tmp))) {
        snprintf(c->pool_host, sizeof(c->pool_host), "%s", tmp);
        c->pool_fetched = true;
    }
    long pool_port = 0;
    if (json_get_number(buf, "pool_port", &pool_port) && pool_port > 0) {
        c->pool_port = (uint16_t)pool_port;
    }
    return 0;
}

int config_save(const Config *c) {
    char buf[2048];
    int n = 0;
    n += snprintf(buf + n, sizeof(buf) - n, "{\n");
    n += snprintf(buf + n, sizeof(buf) - n, "  \"server\": \"%s\",\n", c->server);
    n += snprintf(buf + n, sizeof(buf) - n, "  \"port\": %u,\n", c->port);
    n += snprintf(buf + n, sizeof(buf) - n, "  \"username\": \"%s\",\n", c->username);
    n += snprintf(buf + n, sizeof(buf) - n, "  \"worker\": \"%s\",\n", c->worker);
    n += snprintf(buf + n, sizeof(buf) - n, "  \"mining_key\": \"%s\",\n", c->mining_key);
    n += snprintf(buf + n, sizeof(buf) - n, "  \"donation_addr\": \"%s\",\n", c->donation_addr);
    n += snprintf(buf + n, sizeof(buf) - n, "  \"auto_start\": %s,\n", c->auto_start ? "true" : "false");
    n += snprintf(buf + n, sizeof(buf) - n, "  \"pool_host\": \"%s\",\n", c->pool_host);
    n += snprintf(buf + n, sizeof(buf) - n, "  \"pool_port\": %u\n", c->pool_port);
    n += snprintf(buf + n, sizeof(buf) - n, "}\n");
    return write_file(CONFIG_PATH, buf);
}

const char *config_field_buf(int field) {
    if (field == 0) return g_set_server;
    if (field == 1) return g_set_port;
    if (field == 2) return g_set_user;
    if (field == 3) return g_set_worker;
    if (field == 4) return g_set_key;
    return g_set_donation;
}

char *config_field_buf_write(int field) {
    if (field == 0) return g_set_server;
    if (field == 1) return g_set_port;
    if (field == 2) return g_set_user;
    if (field == 3) return g_set_worker;
    if (field == 4) return g_set_key;
    return g_set_donation;
}

int config_field_max_len(int field) {
    if (field == 1) return 8;
    return 64;
}

void config_commit_field(int field, const char *text) {
    if (field >= 0 && field <= 5) {
        snprintf(config_field_buf_write(field), config_field_max_len(field), "%s", text);
    }
}

void config_set_auto_start(bool enabled) { g_set_auto_start = enabled; }
bool config_get_auto_start(void) { return g_set_auto_start; }

void config_log(const char *fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    pthread_mutex_lock(&g_log_lock);
    for (int i = 7; i > 0; i--) {
        snprintf(g_log_lines[i], 96, "%s", g_log_lines[i - 1]);
    }
    snprintf(g_log_lines[0], 96, "%s", buf);
    if (g_log_count < 8) g_log_count++;
    pthread_mutex_unlock(&g_log_lock);
}

const char *config_log_line(int i) {
    if (i < 0 || i >= 8) return "";
    return g_log_lines[i];
}

int config_log_count(void) {
    return g_log_count;
}
