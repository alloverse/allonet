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
 * - Proxy: The object that runs on the calling thread, probably app's main thread
 * - Bridge: The object that runs on the internal network thread
 * - ProxyClient: Externally responds to the client.h API, but just sends messages over
 *   to the bridge thread.
 * - BridgeClient: Uses the client.c implementation to actually perform the requested 
 *   actions.
 */

typedef enum
{
    msg_connect
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
    } value;
    STAILQ_ENTRY(proxy_message) entries;
} proxy_message;

typedef struct {
    alloclient *bridgeclient;
    thrd_t thr;
    mtx_t messages_mtx;
    bool running;
    STAILQ_HEAD(proxy_message_stailq, proxy_message) messages;
} clientproxy_internal;

static clientproxy_internal *_internal(alloclient *client)
{
    return (clientproxy_internal*)client->_internal;
}

// single underscore function: proxy-side implementation
static bool _alloclient_connect(alloclient *client, const char *url, const char *identity, const char *avatar_desc)
{
    proxy_message *msg = calloc(1, sizeof(proxy_message));
    msg->type = msg_connect;
    msg->value.connect.url = strdup(url);
    msg->value.connect.identity = strdup(identity);
    msg->value.connect.avatar_desc = strdup(avatar_desc);
    
    mtx_lock(&_internal(client)->messages_mtx);
    STAILQ_INSERT_TAIL(&_internal(client)->messages, msg, entries);
    mtx_unlock(&_internal(client)->messages_mtx);
    printf("Added connect message to queue\n");
}
// double underscore function: bridge-side (internal thread) imeplementation
static bool __alloclient_connect(alloclient *client, proxy_message *msg)
{
    printf("Bridge thread: Popped connect message\n");
    bool success = alloclient_connect(_internal(client)->bridgeclient, msg->value.connect.url, msg->value.connect.identity, msg->value.connect.avatar_desc);
    printf("Bridge thread: connection status: %d\n", success);
    free(msg->value.connect.url);
    free(msg->value.connect.identity);
    free(msg->value.connect.avatar_desc);
}

// thread: proxy
static void _alloclient_disconnect(alloclient *client, int reason)
{

}
// thread: bridge
static void __alloclient_disconnect(alloclient *client, int reason)
{

}

// thread: proxy
static bool _alloclient_poll(alloclient *client, int timeout_ms)
{

}
// thread: bridge
static bool __alloclient_poll(alloclient *client, int timeout_ms)
{

}

// thread: proxy
static void _alloclient_send_interaction(alloclient *client, allo_interaction *interaction)
{

}
// thread: bridge
static void __alloclient_send_interaction(alloclient *client, allo_interaction *interaction)
{

}

// thread: proxy
static void _alloclient_set_intent(alloclient *client, allo_client_intent *intent)
{

}
// thread: bridge
static void __alloclient_set_intent(alloclient *client, allo_client_intent *intent)
{

}

// thread: proxy
static void _alloclient_send_audio(alloclient *client, int32_t track_id, const int16_t *pcm, size_t sample_count)
{

}
// thread: bridge
static void __alloclient_send_audio(alloclient *client, int32_t track_id, const int16_t *pcm, size_t sample_count)
{

}

// thread: proxy
static void _alloclient_request_asset(alloclient* client, const char* asset_id, const char* entity_id)
{

}
// thread: bridge
static void __alloclient_request_asset(alloclient* client, const char* asset_id, const char* entity_id)
{

}

// thread: proxy
static void _alloclient_simulate(alloclient* client, double dt)
{

}
// thread: bridge
static void __alloclient_simulate(alloclient* client, double dt)
{

}

// thread: proxy
static double _alloclient_get_time(alloclient* client)
{

}
// thread: bridge
static double __alloclient_get_time(alloclient* client)
{



}


// thread: bridge
static void check_for_messages(alloclient *client)
{
    mtx_lock(&_internal(client)->messages_mtx);
    proxy_message *msg = NULL;
    while((msg = STAILQ_FIRST(&_internal(client)->messages))) {
        STAILQ_REMOVE_HEAD(&_internal(client)->messages, entries);
        switch(msg->type) {
            case msg_connect:
                __alloclient_connect(client, msg);
                break;
        }
        free(msg);
    }
    mtx_unlock(&_internal(client)->messages_mtx);
}

// thread: bridge
static void check_for_network(alloclient *client)
{
    alloclient_poll(_internal(client)->bridgeclient, (1.0/40)*1000);
}

// thread: bridge
static void _bridgethread(alloclient *client)
{
    while(_internal(client)->running)
    {
        check_for_messages(client);
        check_for_network(client);
    }
}

// thread: proxy
alloclient *clientproxy_create(void)
{
    alloclient *client = (alloclient*)calloc(1, sizeof(alloclient));
    client->_internal = calloc(1, sizeof(clientproxy_internal));
    _internal(client)->bridgeclient = alloclient_create(false);
    _internal(client)->running = true;

    LIST_INIT(&client->state.entities);
    STAILQ_INIT(&_internal(client)->messages);

    client->alloclient_connect = _alloclient_connect;
    client->alloclient_disconnect = _alloclient_disconnect;
    client->alloclient_poll = _alloclient_poll;
    client->alloclient_send_interaction = _alloclient_send_interaction;
    client->alloclient_set_intent = _alloclient_set_intent;
    client->alloclient_send_audio = _alloclient_send_audio;
    client->alloclient_simulate = _alloclient_simulate;
    client->alloclient_get_time = _alloclient_get_time;

    // todo: setup callbacks in bridgeclient

    int success = thrd_create(&_internal(client)->thr, (thrd_start_t)_bridgethread, (void*)client);
    assert(success == thrd_success);
    success = mtx_init(&_internal(client)->messages_mtx, mtx_plain);
    assert(success == thrd_success);


    return client;
}