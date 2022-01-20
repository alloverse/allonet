#include "clientproxy.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "threading.h"
#include "util.h"
#include "inlinesys/queue.h"

// internals in client.c
int64_t alloclient_parse_statediff(alloclient *client, cJSON *cmd);
void alloclient_ack_rev(alloclient *client, int64_t rev);

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
    msg_video,
    msg_clock,
    msg_ack,
    
    msg_asset_request,
    msg_asset_send_data,
    msg_asset_state_callback,
    msg_asset_receive_callback,
    msg_asset_request_bytes_callback,
    
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
            uint32_t track_id;
            allopicture *picture;
        } video;
        struct 
        {
            double latency;
            double delta;
        } clock;
        struct
        {
            int64_t rev;
        } ack;
        
        
        struct {
            char *asset_id;
            char *entity_id;
        } asset_request;
        
        struct {
            char *asset_id;
            int state;
        } asset_state_callback;
        
        struct {
            char *asset_id;
            size_t offset, length, total_size;
            uint8_t *data;
        } asset_data;
        
        struct {
            char *asset_id;
            size_t offset, length;
        } asset_request_bytes;
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
    int exit_code;
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
    original_alloclient_disconnect(proxyclient, reason);
}
static void bridge_alloclient_disconnect(alloclient *bridgeclient, proxy_message *msg)
{
    fprintf(stderr, "alloclient[clientproxy]: shutting down network thread...\n");
    alloclient *proxyclient = bridgeclient->_backref;
    _internal(proxyclient)->running = false;
    _internal(proxyclient)->exit_code = msg->value.disconnection.code;
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

static void (*original_alloclient_set_intent)(alloclient *proxyclient, const allo_client_intent *intent);
static void proxy_alloclient_set_intent(alloclient *proxyclient, const allo_client_intent *intent)
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

static void proxy_alloclient_send_video(alloclient *proxyclient, int32_t track_id, allopicture *picture)
{
    proxy_message *msg = proxy_message_create(msg_video);
    msg->value.video.picture = picture;
    msg->value.video.track_id = track_id;
    enqueue_proxy_to_bridge(_internal(proxyclient), msg);
}
static void bridge_alloclient_send_video(alloclient *bridgeclient, proxy_message *msg)
{
    alloclient_send_video(bridgeclient, msg->value.video.track_id, msg->value.video.picture);
}

static void bridge_alloclient_ack(alloclient *bridgeclient, proxy_message *msg)
{
    alloclient_ack_rev(bridgeclient, msg->value.ack.rev);
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
    
    char extra[1024];
    _internal(proxyclient)->bridgeclient->alloclient_get_stats(_internal(proxyclient)->bridgeclient, extra, 1024);

    snprintf(buffer, bufferlen, 
        "p2b %d\nb2p %d\n"
        "%s",
        p2b, b2p, 
        extra
    );
}

static void proxy_alloclient_asset_request(alloclient *proxyclient, const char *asset_id, const char *entity_id) {
    proxy_message *msg = proxy_message_create(msg_asset_request);
    msg->value.asset_request.asset_id = strdup(asset_id);
    msg->value.asset_request.entity_id = entity_id == NULL ? NULL : strdup(entity_id);
    enqueue_proxy_to_bridge(_internal(proxyclient), msg);
}
static void bridge_alloclient_asset_request(alloclient *bridgeclient, proxy_message *msg) {
    alloclient_asset_request(bridgeclient, msg->value.asset_request.asset_id, msg->value.asset_request.entity_id);
    
    free(msg->value.asset_request.asset_id);
    if (msg->value.asset_request.entity_id) {
        free(msg->value.asset_request.entity_id);
    }
}

static void proxy_alloclient_asset_send(alloclient *proxyclient, const char *asset_id, const uint8_t *data, size_t offset, size_t length, size_t total_size) {
    proxy_message *msg = proxy_message_create(msg_asset_send_data);
    msg->value.asset_data.asset_id = strdup(asset_id);
    if (data == NULL) {
        msg->value.asset_data.data = NULL;
    } else {
        msg->value.asset_data.data = malloc(length);
        assert(msg->value.asset_data.data);
        memcpy(msg->value.asset_data.data, data, length);
    }
    msg->value.asset_data.offset = offset;
    msg->value.asset_data.length = length;
    msg->value.asset_data.total_size = total_size;
    enqueue_proxy_to_bridge(_internal(proxyclient), msg);
}
static void bridge_alloclient_asset_send_data(alloclient *bridgeclient, proxy_message *msg) {
    alloclient_asset_send(bridgeclient, msg->value.asset_data.asset_id,
        msg->value.asset_data.data,
        msg->value.asset_data.offset,
        msg->value.asset_data.length,
        msg->value.asset_data.total_size
    );
    
    free(msg->value.asset_data.asset_id);
    if (msg->value.asset_data.data) {
        free(msg->value.asset_data.data);
    }
}



static void(*bridge_message_lookup_table[])(alloclient*, proxy_message*) = {
    [msg_connect] = bridge_alloclient_connect,
    [msg_disconnect] = bridge_alloclient_disconnect,
    [msg_interaction] = bridge_alloclient_send_interaction,
    [msg_intent] = bridge_alloclient_set_intent,
    [msg_audio] = bridge_alloclient_send_audio,
    [msg_video] = bridge_alloclient_send_video,
    [msg_ack] = bridge_alloclient_ack,
    [msg_asset_request] = bridge_alloclient_asset_request,
    [msg_asset_send_data] = bridge_alloclient_asset_send_data,
};

//////// Callbacks

static void bridge_asset_state_callback(alloclient *bridgeclient, const char *asset_id, client_asset_state state) {
    proxy_message *msg = proxy_message_create(msg_asset_state_callback);
    msg->value.asset_state_callback.asset_id = strdup(asset_id);
    msg->value.asset_state_callback.state = state;
    enqueue_bridge_to_proxy(_internal(bridgeclient->_backref), msg);
}
static void proxy_asset_state_callback(alloclient *proxyclient, proxy_message *msg) {
    if (proxyclient->asset_state_callback) {
        proxyclient->asset_state_callback(proxyclient, msg->value.asset_state_callback.asset_id, msg->value.asset_state_callback.state);
    }
    free(msg->value.asset_state_callback.asset_id);
}


//proxyclient->asset_receive_callback = proxy_receive_callback;
static void bridge_asset_receive_callback(alloclient *bridgeclient, const char *asset_id, const uint8_t *data, size_t offset, size_t length, size_t total_size) {
    proxy_message *msg = proxy_message_create(msg_asset_receive_callback);
    msg->value.asset_data.asset_id = strdup(asset_id);
    msg->value.asset_data.offset = offset;
    msg->value.asset_data.length = length;
    msg->value.asset_data.total_size = total_size;
    assert(length > 0);
    assert(data != NULL);
    msg->value.asset_data.data = malloc(length);
    assert(msg->value.asset_data.data);
    memcpy(msg->value.asset_data.data, data, length);
    enqueue_bridge_to_proxy(_internal(bridgeclient->_backref), msg);
}
static void proxy_asset_receive_callback(alloclient *bridgeclient, proxy_message *msg) {
    
    if (bridgeclient->asset_receive_callback) {
        bridgeclient->asset_receive_callback(
            bridgeclient,
            msg->value.asset_data.asset_id,
            msg->value.asset_data.data,
            msg->value.asset_data.offset,
            msg->value.asset_data.length,
            msg->value.asset_data.total_size
        );
    }
    
    free(msg->value.asset_data.asset_id);
    free(msg->value.asset_data.data);
}

//proxyclient->asset_send_callback = proxy_send_callback;
static void bridge_asset_request_bytes_callback(alloclient *bridgeclient, const char *asset_id, size_t offset, size_t length) {
    proxy_message *msg = proxy_message_create(msg_asset_request_bytes_callback);
    msg->value.asset_request_bytes.asset_id = strdup(asset_id);
    msg->value.asset_request_bytes.length = length;
    msg->value.asset_request_bytes.offset = offset;
    
    enqueue_bridge_to_proxy(_internal(bridgeclient->_backref), msg);
}
static void proxy_alloclient_asset_request_bytes_callback(alloclient *proxyclient, proxy_message *msg) {
    if (proxyclient->asset_request_bytes_callback) {
        proxyclient->asset_request_bytes_callback(
            proxyclient,
            msg->value.asset_request_bytes.asset_id,
            msg->value.asset_request_bytes.offset,
            msg->value.asset_request_bytes.length
        );
    }
    
    free(msg->value.asset_request_bytes.asset_id);
}

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
    int64_t rev = alloclient_parse_statediff(proxyclient, msg->value.state_delta);
    // note: parse_statediff takes ownership of state_delta, so no need to free it.

    proxy_message *out = proxy_message_create(msg_ack);
    out->value.ack.rev = rev;
    enqueue_proxy_to_bridge(_internal(proxyclient), out);
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

static bool bridge_video_callback(alloclient *bridgeclient, uint32_t track_id, allopixel pixels[], int32_t pixels_wide, int32_t pixels_high)
{
    alloclient *proxyclient = bridgeclient->_backref;
    proxy_message *msg = proxy_message_create(msg_video);
    msg->value.video.track_id = track_id;
    msg->value.video.picture = calloc(1, sizeof(allopicture));
    msg->value.video.picture->planes[0].rgba = pixels;
    msg->value.video.picture->width = pixels_wide;
    msg->value.video.picture->height = pixels_high;
    enqueue_bridge_to_proxy(_internal(proxyclient), msg);
    return false;
}
static void proxy_video_callback(alloclient *proxyclient, proxy_message *msg)
{
    if(!proxyclient->video_callback || proxyclient->video_callback(proxyclient, msg->value.video.track_id, msg->value.video.picture->planes[0].rgba, msg->value.video.picture->width, msg->value.video.picture->height))
    {
        free(msg->value.video.picture->planes[0].rgba);
        free(msg->value.video.picture);
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
    [msg_video] = proxy_video_callback,
    [msg_disconnect] = proxy_disconnected_callback,
    [msg_clock] = proxy_clock_callback,
    [msg_asset_state_callback] = proxy_asset_state_callback,
    [msg_asset_receive_callback] = proxy_asset_receive_callback,
    [msg_asset_request_bytes_callback] = proxy_alloclient_asset_request_bytes_callback,
};


//////// Thread scaffolding on proxy
// thread: proxy (parsing messages from bridge)
static bool proxy_alloclient_poll(alloclient *proxyclient, int timeout_ms)
{
    (void)timeout_ms;
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
    fprintf(stderr, "alloclient[clientproxy]: deallocating network thread resources...\n");
    alloclient_disconnect(bridgeclient, _internal(bridgeclient->_backref)->exit_code);
}

// thread: proxy
alloclient *clientproxy_create(alloclient *target)
{
    alloclient *proxyclient = alloclient_create(false);
    proxyclient->_internal2 = calloc(1, sizeof(clientproxy_internal));
    _internal(proxyclient)->bridgeclient = target;
    _internal(proxyclient)->bridgeclient->_backref = proxyclient;
    _internal(proxyclient)->bridgeclient->raw_state_delta_callback = bridge_raw_state_delta_callback;
    _internal(proxyclient)->bridgeclient->interaction_callback = bridge_interaction_callback;
    _internal(proxyclient)->bridgeclient->audio_callback = bridge_audio_callback;
    _internal(proxyclient)->bridgeclient->video_callback = bridge_video_callback;
    _internal(proxyclient)->bridgeclient->disconnected_callback = bridge_disconnected_callback;
    _internal(proxyclient)->bridgeclient->clock_callback = bridge_clock_callback;
    _internal(proxyclient)->bridgeclient->asset_state_callback = bridge_asset_state_callback;
    _internal(proxyclient)->bridgeclient->asset_receive_callback = bridge_asset_receive_callback;
    _internal(proxyclient)->bridgeclient->asset_request_bytes_callback = bridge_asset_request_bytes_callback;
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
    proxyclient->alloclient_send_video = proxy_alloclient_send_video;
    proxyclient->alloclient_get_time = proxy_alloclient_get_time;
    proxyclient->alloclient_get_stats = proxy_alloclient_get_stats;
    
    
    proxyclient->alloclient_asset_request = proxy_alloclient_asset_request;
    proxyclient->alloclient_asset_send = proxy_alloclient_asset_send;

    int success = thrd_create(&_internal(proxyclient)->thr, (thrd_start_t)_bridgethread, (void*)_internal(proxyclient)->bridgeclient);
    assert(success == thrd_success);
    success = mtx_init(&_internal(proxyclient)->proxy_to_bridge_mtx, mtx_plain);
    assert(success == thrd_success);
    success = mtx_init(&_internal(proxyclient)->bridge_to_proxy_mtx, mtx_plain);
    assert(success == thrd_success);

    return proxyclient;
}

