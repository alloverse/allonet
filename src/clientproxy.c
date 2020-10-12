#include "clientproxy.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#if __STDC_VERSION__ >= 201112L
#   include <threads.h>
#else
#   include <tinycthread.h>
#endif

#include "inlinesys/queue.h"

/**
 * This file proxies the network client to its own thread.
 * Terminology:
 * - Proxy: The object that runs on the calling thread, probably app's main thread. 
 * - Bridge: The object that runs on the internal network thread.
 * - ProxyClient: Externally responds to the client.h API, but just sends messages over
 *   to the bridge thread.
 * - BridgeClient: Uses the client.c implementation to actually perform the requested 
 *   actions.
 */

typedef enum
{
    msg_connect,
    msg_interaction
} proxy_message_type;

/// Messages to be sent from proxy to bridge
typedef struct proxy_message
{
    proxy_message_type type;
    union
    {
        struct
        {
            char *url;
            char *identity;
            char *avatar_desc;
        } connect;
        allo_interaction *interaction;
    } value;
    STAILQ_ENTRY(proxy_message) entries;
} proxy_message;

typedef STAILQ_HEAD(proxy_message_stailq, proxy_message) proxy_message_stailq;

typedef struct {
    alloclient *bridgeclient;
    thrd_t thr;
    mtx_t proxy_to_bridge_mtx;
    mtx_t bridge_to_proxy_mtx;
    bool running;
    proxy_message_stailq proxy_to_bridge;
    proxy_message_stailq bridge_to_proxy;
} clientproxy_internal;

static clientproxy_internal *_internal(alloclient *client)
{
    return (clientproxy_internal*)client->_internal;
}

static bool proxy_alloclient_connect(alloclient *proxyclient, const char *url, const char *identity, const char *avatar_desc)
{
    proxy_message *msg = calloc(1, sizeof(proxy_message));
    msg->type = msg_connect;
    msg->value.connect.url = strdup(url);
    msg->value.connect.identity = strdup(identity);
    msg->value.connect.avatar_desc = strdup(avatar_desc);
    
    mtx_lock(&_internal(proxyclient)->proxy_to_bridge_mtx);
    STAILQ_INSERT_TAIL(&_internal(proxyclient)->proxy_to_bridge, msg, entries);
    mtx_unlock(&_internal(proxyclient)->proxy_to_bridge_mtx);
}
static bool bridge_alloclient_connect(alloclient *bridgeclient, proxy_message *msg)
{
    bool success = alloclient_connect(bridgeclient, msg->value.connect.url, msg->value.connect.identity, msg->value.connect.avatar_desc);
    free(msg->value.connect.url);
    free(msg->value.connect.identity);
    free(msg->value.connect.avatar_desc);
}

static void proxy_alloclient_disconnect(alloclient *proxyclient, int reason)
{

}
static void bridge_alloclient_disconnect(alloclient *bridgeclient, int reason)
{

}

static void proxy_alloclient_send_interaction(alloclient *proxyclient, allo_interaction *interaction)
{

}
static void bridge_alloclient_send_interaction(alloclient *bridgeclient, allo_interaction *interaction)
{

}

static void proxy_alloclient_set_intent(alloclient *proxyclient, allo_client_intent *intent)
{

}
static void bridge_alloclient_set_intent(alloclient *bridgeclient, allo_client_intent *intent)
{

}

static void proxy_alloclient_send_audio(alloclient *proxyclient, int32_t track_id, const int16_t *pcm, size_t sample_count)
{

}
static void bridge_alloclient_send_audio(alloclient *bridgeclient, int32_t track_id, const int16_t *pcm, size_t sample_count)
{

}

static void proxy_alloclient_request_asset(alloclient* proxyclient, const char* asset_id, const char* entity_id)
{

}
static void bridge_alloclient_request_asset(alloclient* bridgeclient, const char* asset_id, const char* entity_id)
{

}

static void proxy_alloclient_simulate(alloclient* proxyclient, double dt)
{

}
static void bridge_alloclient_simulate(alloclient* bridgeclient, double dt)
{

}

static double proxy_alloclient_get_time(alloclient* proxyclient)
{

}
static double bridge_alloclient_get_time(alloclient* bridgeclient)
{

}


//////// Callbacks

static void bridge_state_callback(alloclient *bridgeclient, allo_state *state)
{
    alloclient *proxyclient = bridgeclient->_backref;
    /*allo_state *swap = proxyclient->_state;
    proxyclient->state = bridgeclient->_state;
    bridgeclient->state = swap;*/
}
static void proxy_state_callback(alloclient *proxyclient, proxy_message *msg)
{

}

static bool bridge_interaction_callback(alloclient *bridgeclient, allo_interaction *interaction)
{
    alloclient *proxyclient = bridgeclient->_backref;
    proxy_message *msg = calloc(1, sizeof(proxy_message));
    msg->type = msg_interaction;
    msg->value.interaction = interaction;
    
    mtx_lock(&_internal(proxyclient)->bridge_to_proxy_mtx);
    STAILQ_INSERT_TAIL(&_internal(proxyclient)->bridge_to_proxy, msg, entries);
    mtx_unlock(&_internal(proxyclient)->bridge_to_proxy_mtx);
    return false;
}
static bool proxy_interaction_callback(alloclient *proxyclient, proxy_message *msg)
{
    if(!proxyclient->interaction_callback || proxyclient->interaction_callback(proxyclient, msg->value.interaction))
    {
        allo_interaction_free(msg->value.interaction);
    }
}

static bool bridge_audio_callback(alloclient *bridgeclient, uint32_t track_id, int16_t pcm[], int32_t samples_decoded)
{

    return false;
}
static bool proxy_audio_callback(alloclient *proxyclient, proxy_message *msg)
{

}

static void bridge_disconnected_callback(alloclient *bridgeclient, alloerror code, const char *message)
{

}
static void proxy_disconnected_callback(alloclient *proxyclient, proxy_message *msg)
{

}


//////// Thread scaffolding on proxy
// thread: proxy (parsing messages from bridge)
static bool proxy_alloclient_poll(alloclient *proxyclient, int timeout_ms)
{
    mtx_lock(&_internal(proxyclient)->bridge_to_proxy_mtx);
    proxy_message *msg = NULL;
    while((msg = STAILQ_FIRST(&_internal(proxyclient)->bridge_to_proxy))) {
        STAILQ_REMOVE_HEAD(&_internal(proxyclient)->bridge_to_proxy, entries);
        switch(msg->type) {
            case msg_interaction:
                proxy_interaction_callback(proxyclient, msg);
                break;
            default: assert(false && "unhandled message");
        }
        free(msg);
    }
    mtx_unlock(&_internal(proxyclient)->bridge_to_proxy_mtx);
}

//////// Thread scaffolding on bridge


// thread: bridge
static void bridge_check_for_messages(alloclient *bridgeclient)
{
    alloclient *proxyclient = bridgeclient->_backref;
    mtx_lock(&_internal(proxyclient)->proxy_to_bridge_mtx);
    proxy_message *msg = NULL;
    while((msg = STAILQ_FIRST(&_internal(proxyclient)->proxy_to_bridge))) {
        STAILQ_REMOVE_HEAD(&_internal(proxyclient)->proxy_to_bridge, entries);
        switch(msg->type) {
            case msg_connect:
                bridge_alloclient_connect(bridgeclient, msg);
                break;
            default: assert(false && "unhandled message");
        }
        free(msg);
    }
    mtx_unlock(&_internal(proxyclient)->proxy_to_bridge_mtx);
}

// thread: bridge
static void bridge_check_for_network(alloclient *bridgeclient)
{
    alloclient_poll(bridgeclient, (1.0/40)*1000);
}

// thread: bridge
static void _bridgethread(alloclient *bridgeclient)
{
    while(_internal(bridgeclient->_backref)->running)
    {
        bridge_check_for_messages(bridgeclient);
        bridge_check_for_network(bridgeclient);
    }
}

// thread: proxy
alloclient *clientproxy_create(void)
{
    alloclient *proxyclient = (alloclient*)calloc(1, sizeof(alloclient));
    proxyclient->_internal = calloc(1, sizeof(clientproxy_internal));
    _internal(proxyclient)->bridgeclient = alloclient_create(false);
    _internal(proxyclient)->bridgeclient->_backref = proxyclient;
    _internal(proxyclient)->bridgeclient->state_callback = bridge_state_callback;
    _internal(proxyclient)->bridgeclient->interaction_callback = bridge_interaction_callback;
    _internal(proxyclient)->bridgeclient->audio_callback = bridge_audio_callback;
    _internal(proxyclient)->bridgeclient->disconnected_callback = bridge_disconnected_callback;
    _internal(proxyclient)->running = true;

    LIST_INIT(&proxyclient->_state.entities);
    STAILQ_INIT(&_internal(proxyclient)->proxy_to_bridge);
    STAILQ_INIT(&_internal(proxyclient)->bridge_to_proxy);

    proxyclient->alloclient_connect = proxy_alloclient_connect;
    proxyclient->alloclient_disconnect = proxy_alloclient_disconnect;
    proxyclient->alloclient_poll = proxy_alloclient_poll;
    proxyclient->alloclient_send_interaction = proxy_alloclient_send_interaction;
    proxyclient->alloclient_set_intent = proxy_alloclient_set_intent;
    proxyclient->alloclient_send_audio = proxy_alloclient_send_audio;
    proxyclient->alloclient_simulate = proxy_alloclient_simulate;
    proxyclient->alloclient_get_time = proxy_alloclient_get_time;

    // todo: setup callbacks in bridgeclient

    int success = thrd_create(&_internal(proxyclient)->thr, (thrd_start_t)_bridgethread, (void*)_internal(proxyclient)->bridgeclient);
    assert(success == thrd_success);
    success = mtx_init(&_internal(proxyclient)->proxy_to_bridge_mtx, mtx_plain);
    assert(success == thrd_success);
    success = mtx_init(&_internal(proxyclient)->bridge_to_proxy_mtx, mtx_plain);
    assert(success == thrd_success);


    return proxyclient;
}