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

// internals in client.c
void alloclient_parse_statediff(alloclient *client, cJSON *cmd);

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
    msg_interaction,
    msg_state_delta,
    msg_disconnect
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
        cJSON *state_delta;
        int disconnection_code;
    } value;
    STAILQ_ENTRY(proxy_message) entries;
} proxy_message;

static proxy_message *proxy_message_create(proxy_message_type type)
{
    proxy_message *msg = calloc(1, sizeof(proxy_message));
    msg->type = type;
    return msg;
}

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
    return (clientproxy_internal*)client->_backref;
}

static void enqueue_proxy_to_bridge(clientproxy_internal *internal, proxy_message *msg)
{
    mtx_lock(&internal->proxy_to_bridge_mtx);
    STAILQ_INSERT_TAIL(&internal->proxy_to_bridge, msg, entries);
    mtx_unlock(&internal->proxy_to_bridge_mtx);
}

static void enqueue_bridge_to_proxy(clientproxy_internal *internal, proxy_message *msg)
{
    mtx_lock(&internal->bridge_to_proxy_mtx);
    STAILQ_INSERT_TAIL(&internal->bridge_to_proxy, msg, entries);
    mtx_unlock(&internal->bridge_to_proxy_mtx);
}

static bool proxy_alloclient_connect(alloclient *proxyclient, const char *url, const char *identity, const char *avatar_desc)
{
    proxy_message *msg = proxy_message_create(msg_connect);
    msg->value.connect.url = strdup(url);
    msg->value.connect.identity = strdup(identity);
    msg->value.connect.avatar_desc = strdup(avatar_desc);
    enqueue_proxy_to_bridge(_internal(proxyclient), msg);
}
static bool bridge_alloclient_connect(alloclient *bridgeclient, proxy_message *msg)
{
    bool success = alloclient_connect(bridgeclient, msg->value.connect.url, msg->value.connect.identity, msg->value.connect.avatar_desc);
    free(msg->value.connect.url);
    free(msg->value.connect.identity);
    free(msg->value.connect.avatar_desc);
}

static void (*original_alloclient_disconnect)(alloclient *proxyclient, int reason);
static void proxy_alloclient_disconnect(alloclient *proxyclient, int reason)
{
    fprintf(stderr, "alloclient[clientproxy]: Shutting down...\n");
    proxy_message *msg = proxy_message_create(msg_disconnect);
    msg->value.disconnection_code = reason;
    enqueue_proxy_to_bridge(_internal(proxyclient), msg);

    thrd_join(_internal(proxyclient)->thr, NULL);
    mtx_destroy(&_internal(proxyclient)->bridge_to_proxy_mtx);
    mtx_destroy(&_internal(proxyclient)->proxy_to_bridge_mtx);
    free(proxyclient->_backref);
    // TODO: clean out the message queues, goddammit.
    original_alloclient_disconnect(proxyclient, msg->value.disconnection_code);
}
static void bridge_alloclient_disconnect(alloclient *bridgeclient, proxy_message *msg)
{
    alloclient *proxyclient = bridgeclient->_backref;
    alloclient_disconnect(bridgeclient, msg->value.disconnection_code);
    _internal(proxyclient)->running = false;
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

static void bridge_raw_state_delta_callback(alloclient *bridgeclient, cJSON *cmd)
{
    // Our strategy here is to parse the deltas on the main thread instead of the network thread.
    // This is a bit much work to do on a main thread, so it would be much nicer to parse on the network
    // thread and somehow do a pointer swap with the main thread. That's too complicated for my
    // poor brain right now, so let's try it this way, and if the performance is good enough,
    // keep it this way.

    alloclient *proxyclient = bridgeclient->_backref;
    proxy_message *msg = proxy_message_create(msg_state_delta);
    msg->value.state_delta = cmd;
    enqueue_bridge_to_proxy(_internal(proxyclient), msg);
}
static void proxy_raw_state_delta_callback(alloclient *proxyclient, proxy_message *msg)
{
    alloclient_parse_statediff(proxyclient, msg->value.state_delta);
    // note: parse_statediff takes ownership of state_delta, so no need to free it.
}

static bool bridge_interaction_callback(alloclient *bridgeclient, allo_interaction *interaction)
{
    alloclient *proxyclient = bridgeclient->_backref;
    proxy_message *msg = proxy_message_create(msg_interaction);
    msg->value.interaction = interaction;
    enqueue_bridge_to_proxy(_internal(proxyclient), msg);
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
            case msg_state_delta:
                proxy_raw_state_delta_callback(proxyclient, msg);
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
            case msg_disconnect:
                bridge_alloclient_disconnect(bridgeclient, msg);
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
        bridge_check_for_network(bridgeclient);
        bridge_check_for_messages(bridgeclient);
    }
    fprintf(stderr, "alloclient[clientproxy]: exiting network thread...\n");
}

// thread: proxy
alloclient *clientproxy_create(void)
{
    alloclient *proxyclient = alloclient_create(false);
    proxyclient->_backref = calloc(1, sizeof(clientproxy_internal));
    _internal(proxyclient)->bridgeclient = alloclient_create(false);
    _internal(proxyclient)->bridgeclient->_backref = proxyclient;
    _internal(proxyclient)->bridgeclient->raw_state_delta_callback = bridge_raw_state_delta_callback;
    _internal(proxyclient)->bridgeclient->interaction_callback = bridge_interaction_callback;
    _internal(proxyclient)->bridgeclient->audio_callback = bridge_audio_callback;
    _internal(proxyclient)->bridgeclient->disconnected_callback = bridge_disconnected_callback;
    _internal(proxyclient)->running = true;

    STAILQ_INIT(&_internal(proxyclient)->proxy_to_bridge);
    STAILQ_INIT(&_internal(proxyclient)->bridge_to_proxy);

    original_alloclient_disconnect = proxyclient->alloclient_disconnect;
    proxyclient->alloclient_connect = proxy_alloclient_connect;
    proxyclient->alloclient_disconnect = proxy_alloclient_disconnect;
    proxyclient->alloclient_poll = proxy_alloclient_poll;
    proxyclient->alloclient_send_interaction = proxy_alloclient_send_interaction;
    proxyclient->alloclient_set_intent = proxy_alloclient_set_intent;
    proxyclient->alloclient_send_audio = proxy_alloclient_send_audio;
    proxyclient->alloclient_simulate = proxy_alloclient_simulate;
    proxyclient->alloclient_get_time = proxy_alloclient_get_time;

    int success = thrd_create(&_internal(proxyclient)->thr, (thrd_start_t)_bridgethread, (void*)_internal(proxyclient)->bridgeclient);
    assert(success == thrd_success);
    success = mtx_init(&_internal(proxyclient)->proxy_to_bridge_mtx, mtx_plain);
    assert(success == thrd_success);
    success = mtx_init(&_internal(proxyclient)->bridge_to_proxy_mtx, mtx_plain);
    assert(success == thrd_success);

    return proxyclient;
}