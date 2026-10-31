#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    char server[128];
    uint16_t port;
    char username[64];
    char worker[64];
    char mining_key[64];
    char donation_addr[64];
    bool auto_start;
    bool pool_fetched;
    char pool_host[128];
    uint16_t pool_port;
} Config;

int config_load(Config *c);
int config_save(const Config *c);
void config_set_defaults(Config *c);

#endif
