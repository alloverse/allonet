#ifndef ALLONET_SERVER_H
#define ALLONET_SERVER_H
#include "state.h"
#include "net.h"

typedef struct {
    allo_client_intent intent;
    char *avatar_entity_id;

    // private
    void *internals;
} alloserver_client;

typedef struct {
    // call this to update world state and broadcast it
    void (*set_state)(allo_state state);
    
    // send interaction request to entity owned by client
    void (*interact)(alloserver_client *client, const char *entity_id, const char *cmd);
    
    // force disconnect a client
    void (*disconnect)(alloserver_client *client);
    
    // disconnect all clients and stop listening
    void (*stop)(void);
    
    // set this to get a callback when intent changes for a client. data in 'client'
    // is valid only during duration of callback.
    void (*intent_callback)(alloserver_client *client);
    // list of clients changed
    void (*clients_callback)(alloserver_client *clients, int client_count);
    // interaction request from a client
    void (*interaction_callback)(alloserver_client *client, const char *entity_id, const char *cmd);

    // internal
    void (*send)(alloserver_client *client, allo_sendmode mode, const char *buf, int len);
    allo_state state;
    alloserver_client *clients;
    int client_count;
} alloserver;

alloserver *allo_listen(void);

#endif
