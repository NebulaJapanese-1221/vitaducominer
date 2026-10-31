#include "config.h"
#include "json.h"
#include <psp2/io/fcntl.h>
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/threadmgr.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define CONFIG_PATH "ux0:/vitaducominer/config.json"

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
    }
    long port = 0;
    if (json_get_number(buf, "port", &port) && port > 0) {
        c->port = (uint16_t)port;
    }
    if (json_get_string(buf, "username", tmp, sizeof(tmp))) {
        snprintf(c->username, sizeof(c->username), "%s", tmp);
    }
    if (json_get_string(buf, "worker", tmp, sizeof(tmp))) {
        snprintf(c->worker, sizeof(c->worker), "%s", tmp);
    }
    if (json_get_string(buf, "mining_key", tmp, sizeof(tmp))) {
        snprintf(c->mining_key, sizeof(c->mining_key), "%s", tmp);
    }
    if (json_get_string(buf, "donation_addr", tmp, sizeof(tmp))) {
        snprintf(c->donation_addr, sizeof(c->donation_addr), "%s", tmp);
    }
    long auto_start = 0;
    if (json_get_number(buf, "auto_start", &auto_start)) {
        c->auto_start = (auto_start != 0);
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
