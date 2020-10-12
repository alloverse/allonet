#include "clientproxy.h"
#include <stdlib.h>
#include <tinycthread.h>

typedef struct {
    alloclient *bridgeclient;
    thrd_t thr;
} clientproxy_internal;

static clientproxy_internal *_internal(alloclient *client)
{
    return (clientproxy_internal*)client->_internal;
}

// single underscore function: proxy-side implementation
static bool _alloclient_connect(alloclient *client, const char *url, const char *identity, const char *avatar_desc)
{

}

static void _bridgethread(alloclient *client)
{

}


alloclient *clientproxy_create(void)
{
    alloclient *client = (alloclient*)calloc(1, sizeof(alloclient));
    client->_internal = calloc(1, sizeof(clientproxy_internal));
    _internal(client)->bridgeclient = alloclient_create(false);

    LIST_INIT(&client->state.entities);

    client->alloclient_connect = _alloclient_connect;
/*    client->alloclient_disconnect = _alloclient_disconnect;
    client->alloclient_poll = _alloclient_poll;
    client->alloclient_send_interaction = _alloclient_send_interaction;
    client->alloclient_set_intent = _alloclient_set_intent;
    client->alloclient_send_audio = _alloclient_send_audio;
    client->alloclient_simulate = _alloclient_simulate;
    client->alloclient_get_time = _alloclient_get_time;*/

    thrd_create(&_internal(client)->thr, (thrd_start_t)_bridgethread, (void*)client);

    return client;
}