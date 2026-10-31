#ifndef MINER_H
#define MINER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <pthread.h>

typedef struct {
    char username[64];
    char mining_key[64];
    char server[64];
    uint16_t port;
    uint32_t difficulty;
    bool connected;
    bool mining;
    bool running;
    bool pool_fetched;
    uint64_t hashes;
    double hashrate;
    uint64_t shares_accepted;
    uint64_t shares_rejected;
    uint32_t nonce;
    uint8_t last_digest[20];
    uint64_t uptime_ms;
    uint64_t start_time_ms;
} Miner;

pthread_t *miner_start(void);
void miner_stop(void);
void miner_join(void);
Miner *miner_get_stats(void);
void miner_set_server(const char *server, uint16_t port);
void miner_set_username(const char *username);
void miner_set_mining_key(const char *key);
bool miner_is_mining(void);
void miner_toggle_mining(void);
int miner_fetch_pool(void);
const char *miner_pool_host(void);
uint16_t miner_pool_port(void);
bool miner_pool_valid(void);

#endif
