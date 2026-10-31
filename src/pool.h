#ifndef POOL_H
#define POOL_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    char name[64];
    char host[128];
    uint16_t port;
    bool valid;
} PoolInfo;

int pool_fetch(PoolInfo *pool);
bool pool_is_valid(const PoolInfo *pool);

#endif
