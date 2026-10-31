#include "pool.h"
#include "http.h"
#include "json.h"
#include <string.h>
#include <stdio.h>

#define GETPOOL_URL "http://server.duinocoin.com/getPool"

int pool_fetch(PoolInfo *pool) {
    memset(pool, 0, sizeof(*pool));
    snprintf(pool->name, sizeof(pool->name), "auto");
    snprintf(pool->host, sizeof(pool->host), "server.duinocoin.com");
    pool->port = 12181;
    pool->valid = false;

    char buf[2048];
    int status = 0;
    int len = http_get(GETPOOL_URL, buf, sizeof(buf), &status);
    if (len <= 0 || status != 200) {
        return -1;
    }

    char host[128] = {0};
    long port = 12181;
    char name[64] = {0};

    json_get_string(buf, "host", host, sizeof(host));
    if (json_get_number(buf, "port", &port)) {
        if (port > 0 && port < 65536) {
            pool->port = (uint16_t)port;
        }
    }
    if (json_get_string(buf, "name", name, sizeof(name)) && name[0]) {
        snprintf(pool->name, sizeof(pool->name), "%s", name);
    }

    if (host[0]) {
        snprintf(pool->host, sizeof(pool->host), "%s", host);
        pool->port = (uint16_t)port;
        pool->valid = true;
        return 0;
    }

    return -1;
}

bool pool_is_valid(const PoolInfo *pool) {
    return pool->valid;
}
