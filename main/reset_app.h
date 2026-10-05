#pragma once
#include "reset_model.h"
typedef enum { NET_STARTING, NET_SETUP, NET_CONNECTING, NET_SYNCING, NET_ONLINE, NET_OFFLINE, NET_ERROR } reset_net_state_t;
typedef struct {
    reset_status_t status;
    challenge_t challenge;
    reset_net_state_t network;
    bool setup;
    char ap_name[33], ap_password[17], message[64];
} reset_snapshot_t;
void reset_publish(const reset_snapshot_t *snapshot);
void reset_network_start(void);
void reset_network_request(bool setup);
