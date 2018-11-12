#ifndef ALLONET_CLIENT_H
#define ALLONET_CLIENT_H
#include "state.h"
#include "net.h"

typedef struct alloclient alloclient;
typedef struct alloclient {
    // call this to change this client's movement/action intent
    void (*set_intent)(alloclient *client, allo_client_intent intent);
    // rpc on another entity
    void (*interact)(alloclient *client, const char *entity_id, const char *cmd);
    void (*disconnect)(alloclient *client);

    void (*poll)(alloclient *client);
    
    // set this to get a callback when state changes. data in 'state'
    // is valid only during duration of callback.
    void (*state_callback)(alloclient *client, allo_state *state);
    void (*interaction_callback)(alloclient *client, const char *sender_entity_id, const char *receiver_entity_id, const char *cmd);

    // internal
    allo_state state;
    void *_internal;
} alloclient;

alloclient *allo_connect(const char *url);

#endif
