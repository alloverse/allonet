#include "clientproxy.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <tinycthread.h>
#include "util.h"
#include "inlinesys/queue.h"

// internals in client.c
void alloclient_parse_statediff(alloclient *client, cJSON *cmd);
// forwards in this file
static void bridge_disconnected_callback(alloclient *bridgeclient, alloerror code, const char *message);

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
    msg_intent,
    msg_state_delta,
    msg_disconnect,
    msg_audio,
    msg_clock,
    
    msg_asset_request,
    msg_asset_assimilate,

    msg_count
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
        allo_client_intent *intent;
        cJSON *state_delta;
        struct {
            int code;
            char *msg;
        } disconnection;
        struct 
        {
            int32_t track_id;
            int16_t *pcm;
            size_t sample_count;
        } audio;
        struct 
        {
            double latency;
            double delta;
        } clock;
        
        struct {
            char *asset_id;
            char *entity_id;
        } asset_request;
        
        struct {
            char *folder;
        } asset_assimilate;
        
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
    int32_t proxy_to_bridge_len;
    proxy_message_stailq bridge_to_proxy;
    int32_t bridge_to_proxy_len;
} clientproxy_internal;

static clientproxy_internal *_internal(alloclient *client)
{
    return (clientproxy_internal*)client->_internal2;
}

static void enqueue_proxy_to_bridge(clientproxy_internal *internal, proxy_message *msg)
{
    mtx_lock(&internal->proxy_to_bridge_mtx);
    STAILQ_INSERT_TAIL(&internal->proxy_to_bridge, msg, entries);
    internal->proxy_to_bridge_len++;
    mtx_unlock(&internal->proxy_to_bridge_mtx);
}

static void enqueue_bridge_to_proxy(clientproxy_internal *internal, proxy_message *msg)
{
    mtx_lock(&internal->bridge_to_proxy_mtx);
    STAILQ_INSERT_TAIL(&internal->bridge_to_proxy, msg, entries);
    internal->bridge_to_proxy_len++;
    mtx_unlock(&internal->bridge_to_proxy_mtx);
}

static bool proxy_alloclient_connect(alloclient *proxyclient, const char *url, const char *identity, const char *avatar_desc)
{
    proxy_message *msg = proxy_message_create(msg_connect);
    msg->value.connect.url = strdup(url);
    msg->value.connect.identity = strdup(identity);
    msg->value.connect.avatar_desc = strdup(avatar_desc);
    enqueue_proxy_to_bridge(_internal(proxyclient), msg);
    return true;
}
static void bridge_alloclient_connect(alloclient *bridgeclient, proxy_message *msg)
{
    bool success = alloclient_connect(bridgeclient, msg->value.connect.url, msg->value.connect.identity, msg->value.connect.avatar_desc);
    free(msg->value.connect.url);
    free(msg->value.connect.identity);
    free(msg->value.connect.avatar_desc);

    if(!success)
    {
        bridge_disconnected_callback(bridgeclient, alloerror_failed_to_connect, "Failed to connect");
    }
}

static void (*original_alloclient_disconnect)(alloclient *proxyclient, int reason);
static void proxy_alloclient_disconnect(alloclient *proxyclient, int reason)
{
    fprintf(stderr, "alloclient[clientproxy]: Shutting down...\n");
    proxy_message *msg = proxy_message_create(msg_disconnect);
    msg->value.disconnection.code = reason;
    enqueue_proxy_to_bridge(_internal(proxyclient), msg);

    thrd_join(_internal(proxyclient)->thr, NULL);
    mtx_destroy(&_internal(proxyclient)->bridge_to_proxy_mtx);
    mtx_destroy(&_internal(proxyclient)->proxy_to_bridge_mtx);
    free(proxyclient->_internal2);
    // TODO: clean out the message queues, goddammit.
    original_alloclient_disconnect(proxyclient, msg->value.disconnection.code);
}
static void bridge_alloclient_disconnect(alloclient *bridgeclient, proxy_message *msg)
{
    alloclient *proxyclient = bridgeclient->_backref;
    alloclient_disconnect(bridgeclient, msg->value.disconnection.code);
    _internal(proxyclient)->running = false;
}

static void proxy_alloclient_send_interaction(alloclient *proxyclient, allo_interaction *interaction)
{
    proxy_message *msg = proxy_message_create(msg_interaction);
    msg->value.interaction = allo_interaction_clone(interaction);
    enqueue_proxy_to_bridge(_internal(proxyclient), msg);
}
static void bridge_alloclient_send_interaction(alloclient *bridgeclient, proxy_message *msg)
{
    alloclient_send_interaction(bridgeclient, msg->value.interaction);
    allo_interaction_free(msg->value.interaction);
}

static void (*original_alloclient_set_intent)(alloclient *proxyclient, allo_client_intent *intent);
static void proxy_alloclient_set_intent(alloclient *proxyclient, allo_client_intent *intent)
{
    original_alloclient_set_intent(proxyclient, intent);
    proxy_message *msg = proxy_message_create(msg_intent);
    msg->value.intent = allo_client_intent_create();
    allo_client_intent_clone(intent, msg->value.intent);
    enqueue_proxy_to_bridge(_internal(proxyclient), msg);
}
static void bridge_alloclient_set_intent(alloclient *bridgeclient, proxy_message *msg)
{
    alloclient_set_intent(bridgeclient, msg->value.intent);
    allo_client_intent_free(msg->value.intent);
}

static void proxy_alloclient_send_audio(alloclient *proxyclient, int32_t track_id, const int16_t *pcm, size_t sample_count)
{
    proxy_message *msg = proxy_message_create(msg_audio);
    msg->value.audio.pcm = calloc(sizeof(int16_t), sample_count);
    memcpy(msg->value.audio.pcm, pcm, sizeof(int16_t)*sample_count);
    msg->value.audio.sample_count = sample_count;
    msg->value.audio.track_id = track_id;
    enqueue_proxy_to_bridge(_internal(proxyclient), msg);
}
static void bridge_alloclient_send_audio(alloclient *bridgeclient, proxy_message *msg)
{
    alloclient_send_audio(bridgeclient, msg->value.audio.track_id, msg->value.audio.pcm, msg->value.audio.sample_count);
    // in the best of worlds, we'd use a pool of buffers here to avoid malloc churn
    free(msg->value.audio.pcm);
}

static double proxy_alloclient_get_time(alloclient *proxyclient)
{
    return get_ts_monod() + proxyclient->clock_deltaToServer;
}

static void proxy_alloclient_get_stats(alloclient *proxyclient, char *buffer, size_t bufferlen)
{
    mtx_lock(&_internal(proxyclient)->proxy_to_bridge_mtx);
    int p2b = _internal(proxyclient)->proxy_to_bridge_len;
    mtx_unlock(&_internal(proxyclient)->proxy_to_bridge_mtx);
    mtx_lock(&_internal(proxyclient)->bridge_to_proxy_mtx);
    int b2p = _internal(proxyclient)->bridge_to_proxy_len;
    mtx_unlock(&_internal(proxyclient)->bridge_to_proxy_mtx);

    snprintf(buffer, bufferlen, "p2b %d\nb2p %d", p2b, b2p);
}

static void proxy_alloclient_add_asset(alloclient *proxyclient, const char *folder) {
    proxy_message *msg = proxy_message_create(msg_asset_assimilate);
    msg->value.asset_assimilate.folder = strdup(folder);
    enqueue_proxy_to_bridge(_internal(proxyclient), msg);
}
static void bridge_alloclient_add_asset(alloclient *bridgeclient, proxy_message *msg) {
    alloclient_add_asset(bridgeclient, msg->value.asset_assimilate.folder);
    free(msg->value.asset_assimilate.folder);
}

static void proxy_alloclient_request_asset(alloclient *proxyclient, const char *asset_id, const char *entity_id) {
    proxy_message *msg = proxy_message_create(msg_asset_request);
    msg->value.asset_request.asset_id = strdup(asset_id);
    msg->value.asset_request.entity_id = entity_id == NULL ? NULL : strdup(entity_id);
    enqueue_proxy_to_bridge(_internal(proxyclient), msg);
}
static void bridge_alloclient_request_asset(alloclient *bridgeclient, proxy_message *msg) {
    alloclient_request_asset(bridgeclient, msg->value.asset_request.asset_id, msg->value.asset_request.entity_id);
    
    free(msg->value.asset_request.asset_id);
    if (msg->value.asset_request.entity_id) {
        free(msg->value.asset_request.entity_id);
    }
}

static void(*bridge_message_lookup_table[])(alloclient*, proxy_message*) = {
    [msg_connect] = bridge_alloclient_connect,
    [msg_disconnect] = bridge_alloclient_disconnect,
    [msg_interaction] = bridge_alloclient_send_interaction,
    [msg_intent] = bridge_alloclient_set_intent,
    [msg_audio] = bridge_alloclient_send_audio,
    [msg_asset_assimilate] = bridge_alloclient_add_asset,
    [msg_asset_request] = bridge_alloclient_request_asset,
};

//////// Callbacks

static void bridge_raw_state_delta_callback(alloclient *bridgeclient, cJSON *cmd)
{
    // Our strategy here is to parse the deltas on the main thread instead of the network thread.
    // This is a bit much work to do on a main thread, so it would be much nicer to parse on the network
    // thread and somehow do a pointer swap with the main thread. That's too complicated for my
    // poor brain right now, so let's try it this way, and if the performance is good enough,
    // keep it this way.
    assert(cmd);
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
static void proxy_interaction_callback(alloclient *proxyclient, proxy_message *msg)
{
    if(!proxyclient->interaction_callback || proxyclient->interaction_callback(proxyclient, msg->value.interaction))
    {
        allo_interaction_free(msg->value.interaction);
    }
}

static bool bridge_audio_callback(alloclient *bridgeclient, uint32_t track_id, int16_t pcm[], int32_t samples_decoded)
{
    alloclient *proxyclient = bridgeclient->_backref;
    proxy_message *msg = proxy_message_create(msg_audio);
    msg->value.audio.pcm = pcm;
    msg->value.audio.sample_count = samples_decoded;
    msg->value.audio.track_id = track_id;
    enqueue_bridge_to_proxy(_internal(proxyclient), msg);
    return false;
}
static void proxy_audio_callback(alloclient *proxyclient, proxy_message *msg)
{
    if(!proxyclient->audio_callback || proxyclient->audio_callback(proxyclient, msg->value.audio.track_id, msg->value.audio.pcm, msg->value.audio.sample_count))
    {
        free(msg->value.audio.pcm);
    }
}

static void bridge_disconnected_callback(alloclient *bridgeclient, alloerror code, const char *message)
{
    alloclient *proxyclient = bridgeclient->_backref;
    proxy_message *msg = proxy_message_create(msg_disconnect);
    msg->value.disconnection.code = code;
    msg->value.disconnection.msg = allo_strdup(message);
    enqueue_bridge_to_proxy(_internal(proxyclient), msg);
}
static void proxy_disconnected_callback(alloclient *proxyclient, proxy_message *msg)
{
    if(proxyclient->disconnected_callback)
    {
        proxyclient->disconnected_callback(proxyclient, msg->value.disconnection.code, msg->value.disconnection.msg);
    }
    free(msg->value.disconnection.msg);
}

static void bridge_clock_callback(alloclient *bridgeclient, double latency, double delta)
{
    alloclient *proxyclient = bridgeclient->_backref;
    proxy_message *msg = proxy_message_create(msg_clock);
    msg->value.clock.latency = latency;
    msg->value.clock.delta = delta;
    enqueue_bridge_to_proxy(_internal(proxyclient), msg);
}
static void proxy_clock_callback(alloclient *proxyclient, proxy_message *msg)
{
    proxyclient->clock_latency = msg->value.clock.latency;
    proxyclient->clock_deltaToServer = msg->value.clock.delta;
    if(proxyclient->clock_callback) {
        proxyclient->clock_callback(proxyclient, proxyclient->clock_latency, proxyclient->clock_deltaToServer);
    }
}

static void(*proxy_message_lookup_table[])(alloclient*, proxy_message*) = {
    [msg_state_delta] = proxy_raw_state_delta_callback,
    [msg_interaction] = proxy_interaction_callback,
    [msg_audio] = proxy_audio_callback,
    [msg_disconnect] = proxy_disconnected_callback,
    [msg_clock] = proxy_clock_callback,
};


//////// Thread scaffolding on proxy
// thread: proxy (parsing messages from bridge)
static bool proxy_alloclient_poll(alloclient *proxyclient, int timeout_ms)
{
    mtx_lock(&_internal(proxyclient)->bridge_to_proxy_mtx);
    proxy_message *msg = NULL;
    while((msg = STAILQ_FIRST(&_internal(proxyclient)->bridge_to_proxy))) {
        STAILQ_REMOVE_HEAD(&_internal(proxyclient)->bridge_to_proxy, entries);
        _internal(proxyclient)->bridge_to_proxy_len--;
        bool was_disconnect = msg->type == msg_disconnect;
        void (*callback)(alloclient*, proxy_message*) = proxy_message_lookup_table[msg->type];
        assert(callback != NULL && "missing proxy callback");
        callback(proxyclient, msg);
        free(msg);
        if(was_disconnect)
        {
            break;
        }
    }
    mtx_unlock(&_internal(proxyclient)->bridge_to_proxy_mtx);
    return true;
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
        _internal(proxyclient)->proxy_to_bridge_len--;
        void (*callback)(alloclient*, proxy_message*) = bridge_message_lookup_table[msg->type];
        assert(callback != NULL && "missing bridge callback");
        callback(bridgeclient, msg);
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
    proxyclient->_internal2 = calloc(1, sizeof(clientproxy_internal));
    _internal(proxyclient)->bridgeclient = alloclient_create(false);
    _internal(proxyclient)->bridgeclient->_backref = proxyclient;
    _internal(proxyclient)->bridgeclient->raw_state_delta_callback = bridge_raw_state_delta_callback;
    _internal(proxyclient)->bridgeclient->interaction_callback = bridge_interaction_callback;
    _internal(proxyclient)->bridgeclient->audio_callback = bridge_audio_callback;
    _internal(proxyclient)->bridgeclient->disconnected_callback = bridge_disconnected_callback;
    _internal(proxyclient)->bridgeclient->clock_callback = bridge_clock_callback;
    _internal(proxyclient)->running = true;

    STAILQ_INIT(&_internal(proxyclient)->proxy_to_bridge);
    STAILQ_INIT(&_internal(proxyclient)->bridge_to_proxy);

    original_alloclient_disconnect = proxyclient->alloclient_disconnect;
    original_alloclient_set_intent = proxyclient->alloclient_set_intent;
    proxyclient->alloclient_connect = proxy_alloclient_connect;
    proxyclient->alloclient_disconnect = proxy_alloclient_disconnect;
    proxyclient->alloclient_poll = proxy_alloclient_poll;
    proxyclient->alloclient_send_interaction = proxy_alloclient_send_interaction;
    proxyclient->alloclient_set_intent = proxy_alloclient_set_intent;
    proxyclient->alloclient_send_audio = proxy_alloclient_send_audio;
    proxyclient->alloclient_get_time = proxy_alloclient_get_time;
    proxyclient->alloclient_get_stats = proxy_alloclient_get_stats;

    int success = thrd_create(&_internal(proxyclient)->thr, (thrd_start_t)_bridgethread, (void*)_internal(proxyclient)->bridgeclient);
    assert(success == thrd_success);
    success = mtx_init(&_internal(proxyclient)->proxy_to_bridge_mtx, mtx_plain);
    assert(success == thrd_success);
    success = mtx_init(&_internal(proxyclient)->bridge_to_proxy_mtx, mtx_plain);
    assert(success == thrd_success);

    return proxyclient;
}
