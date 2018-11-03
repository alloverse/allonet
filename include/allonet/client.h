#ifndef ALLONET_CLIENT_H
#define ALLONET_CLIENT_H
#include "state.h"
#include "net.h"

typedef struct {
    // call this to change this client's movement/action intent
    void (*set_intent)(allo_client_intent intent);
    // rpc on another entity
    void (*interact)(const char *entity_id, const char *cmd);
    void (*disconnect)(void);
    
    // set this to get a callback when state changes. data in 'state'
    // is valid only during duration of callback.
    void (*state_callback)(allo_state *state);
    void (*interaction_callback)(const char *sender_entity_id, const char *receiver_entity_id, const char *cmd);

    // internal
    void (*send)(allo_sendmode mode, const char *buf, int len);
    allo_state state;
} alloclient;

alloclient *allo_connect(const char *url);

#endif
