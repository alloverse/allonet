#ifndef ALLONET_SERVER_H
#define ALLONET_SERVER_H
#include "inlinesys/queue.h"
#include "state.h"
#include "net.h"

static const int allo_udp_port = 21337;
static const int allo_client_count_max = 128;

typedef struct alloserver_client {
    allo_client_intent intent;
    char *avatar_entity_id;

    // private
    void *_internal;
    void *_backref;
    LIST_ENTRY(alloserver_client) pointers;
} alloserver_client;

typedef struct alloserver alloserver;
struct alloserver {
    // call this to update world state, which will then be sent during heartbeat.
    void (*set_state)(alloserver *serv, allo_state state);
    
    // send interaction request to entity owned by client; sent immediately
    void (*interact)(alloserver *serv, alloserver_client *client, const char *from_entity, const char *to_entity, const char *cmd);
    
    // disconnect a client
    void (*disconnect)(alloserver *serv, alloserver_client *client);
    
    // gracefully disconnect all clients and stop listening, and free `serv`. blocks.
    void (*stop)(alloserver *serv, int timeout_ms);

    // handle incoming events for at most duration_ms. returns true if event was handled
    bool (*interbeat)(alloserver *serv, int duration_ms);
    
    // raw json as delivered from client (intent or interaction)
    void (*raw_indata_callback)(alloserver *serv, alloserver_client *client, allochannel channel, const uint8_t *data, size_t data_length);
    
    // list of clients changed; either `added` or `removed` is set.
    void (*clients_callback)(alloserver *serv, alloserver_client *added, alloserver_client *removed);
    // interaction request from a client
    void (*interaction_callback)(alloserver *serv, alloserver_client *client, const char *from_entity, const char *to_entity, const char *cmd);

    // internal
    void (*send)(alloserver *serv, alloserver_client *client, allochannel channel, const uint8_t *buf, int len);
    allo_state state;
    LIST_HEAD(alloserver_client_list, alloserver_client) clients;

    void *_backref; // use this as a backref for callbacks
    void *_internal; // used within server.c to hide impl
};

alloserver *allo_listen(void);

// internal
int allo_socket_for_select(alloserver *server);

#endif
