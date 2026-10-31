#include "json.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static int is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static char *skip_ws(char *p) {
    while (*p && is_ws(*p)) p++;
    return p;
}

static int match_key(const char **p, const char *key) {
    const char *s = *p;
    s = skip_ws((char *)s);
    if (*s != '"') return 0;
    s++;
    size_t klen = strlen(key);
    size_t i = 0;
    while (*s && *s != '"' && i < klen) {
        if (*s == key[i]) { i++; s++; }
        else return 0;
    }
    if (i != klen || *s != '"') return 0;
    s++; // skip closing quote
    s = skip_ws((char *)s);
    if (*s != ':') return 0;
    s++; // skip ':'
    s = skip_ws((char *)s);
    *p = s;
    return 1;
}

static char *read_string(char *p, char *out, int out_size) {
    if (*p != '"') { out[0] = 0; return p; }
    p++;
    int i = 0;
    while (*p && *p != '"') {
        if (*p == '\\' && *(p + 1)) {
            p++;
            switch (*p) {
                case '"': out[i] = '"'; break;
                case '\\': out[i] = '\\'; break;
                case '/': out[i] = '/'; break;
                case 'n': out[i] = '\n'; break;
                case 't': out[i] = '\t'; break;
                case 'r': out[i] = '\r'; break;
                default: out[i] = *p; break;
            }
        } else {
            out[i] = *p;
        }
        p++;
        i++;
        if (i >= out_size - 1) break;
    }
    out[i] = 0;
    if (*p == '"') p++;
    return p;
}

int json_get_string(const char *json, const char *key, char *out, int out_size) {
    const char *p = json;
    while (*p) {
        if (*p == '"') {
            const char *save = p;
            if (match_key(&p, key)) {
                char tmp[512];
                read_string((char *)p, tmp, sizeof(tmp));
                snprintf(out, out_size, "%s", tmp);
                return 1;
            }
            p = save;
        }
        p++;
    }
    out[0] = 0;
    return 0;
}

int json_get_number(const char *json, const char *key, long *out) {
    const char *p = json;
    while (*p) {
        if (*p == '"') {
            const char *save = p;
            if (match_key(&p, key)) {
                p = skip_ws((char *)p);
                long val = strtol(p, NULL, 10);
                *out = val;
                return 1;
            }
            p = save;
        }
        p++;
    }
    *out = 0;
    return 0;
}
