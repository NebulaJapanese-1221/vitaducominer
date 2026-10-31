#include "http.h"
#include <psp2/net/http.h>
#include <psp2/kernel/threadmgr.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static int g_http_inited = 0;

static void skip_scheme(const char **p) {
    const char *s = *p;
    while (*s && *s != ':') s++;
    if (*s == ':') {
        s++; // skip ':'
        if (*s == '/') s++;
        if (*s == '/') s++;
    }
    *p = s;
}

static const char *find_host_end(const char *s) {
    while (*s && *s != '/' && *s != ':' && *s != '?' && *s != '#') s++;
    return s;
}

int http_init(void) {
    if (g_http_inited) return 0;
    int ret = sceHttpInit(2);
    if (ret < 0 && ret != (int)SCE_HTTP_ERROR_ALREADY_INITED) {
        return ret;
    }
    g_http_inited = 1;
    return 0;
}

void http_term(void) {
    if (g_http_inited) {
        sceHttpTerm();
        g_http_inited = 0;
    }
}

int http_get(const char *url, char *out_buf, size_t out_size, int *status_out) {
    if (!g_http_inited) {
        http_init();
    }

    const char *p = url;
    skip_scheme(&p);

    const char *host_start = p;
    const char *host_end = find_host_end(p);
    size_t host_len = (size_t)(host_end - host_start);
    if (host_len == 0 || host_len >= 256) {
        return -1;
    }
    char host[256];
    memcpy(host, host_start, host_len);
    host[host_len] = 0;

    unsigned short port = 80;
    if (*host_end == ':') {
        host_end++;
        port = (unsigned short)strtoul(host_end, NULL, 10);
        while (*host_end && *host_end != '/' && *host_end != '?' && *host_end != '#') host_end++;
    }

    const char *path = host_end;
    if (*path != '/') {
        path = "/";
    }

    int tmpl = sceHttpCreateTemplate("VitaDucoMiner/1.0", SCE_HTTP_VERSION_1_1, 0);
    if (tmpl < 0) return tmpl;

    int conn = sceHttpCreateConnection(tmpl, host, "http", port, 0);
    if (conn < 0) {
        sceHttpDeleteTemplate(tmpl);
        return conn;
    }

    int req = sceHttpCreateRequest(conn, SCE_HTTP_METHOD_GET, path, 0);
    if (req < 0) {
        sceHttpDeleteConnection(conn);
        sceHttpDeleteTemplate(tmpl);
        return req;
    }

    int ret = sceHttpSendRequest(req, NULL, 0);
    if (ret < 0) {
        sceHttpDeleteRequest(req);
        sceHttpDeleteConnection(conn);
        sceHttpDeleteTemplate(tmpl);
        return ret;
    }

    int status = 0;
    sceHttpGetStatusCode(req, &status);
    if (status_out) *status_out = status;

    size_t total = 0;
    while (total < out_size - 1) {
        int rd = sceHttpReadData(req, out_buf + total, out_size - 1 - total);
        if (rd <= 0) break;
        total += (size_t)rd;
    }
    out_buf[total] = 0;

    sceHttpDeleteRequest(req);
    sceHttpDeleteConnection(conn);
    sceHttpDeleteTemplate(tmpl);

    return (int)total;
}
