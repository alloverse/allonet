#include <allonet/client.h>
#include <allonet/jobs.h>
#include <opus.h>
#include <enet/enet.h>
#include <cJSON/cJSON.h>
#include "../delta.h"
#include "../media/media.h"
#include <allonet/assetstore.h>

typedef struct interaction_queue {
    allo_interaction *interaction;
    LIST_ENTRY(interaction_queue) pointers;
} interaction_queue;


typedef struct {
    mtx_t lock;
    allo_media_track_list media_tracks;
} alloclient_internal_shared;

typedef struct {
    ENetHost *host;
    ENetPeer *peer;
    OpusEncoder *opus_encoder;
    allo_client_intent *latest_intent;
    int64_t latest_intent_ts;
    int64_t latest_clockreq_ts;
    char *avatar_id;
    statehistory_t history;
    scheduler jobs;
    assetstore assets; // asset state tracking
    alloclient_internal_shared *shared; // only use this and derivates thereof within a  _alloclient_internal_shared_begin/_alloclient_internal_shared_end block
} alloclient_internal;

static inline alloclient_internal *_internal(alloclient *client)
{
    return (alloclient_internal*)client->_internal;
}

/// Creates a track for track_id unless one is not already created
extern void _alloclient_media_track_find_or_create(alloclient *client, uint32_t track_id, allo_media_track_type type, const cJSON *comp);
/// Free up track resources
extern void _alloclient_media_track_destroy(alloclient *client, uint32_t track_id);
extern void _alloclient_parse_media(alloclient *client, unsigned char *data, size_t length);
extern void _alloclient_send_audio(alloclient *client, int32_t track_id, const int16_t *pcm, size_t frameCount);
extern void _alloclient_send_video(alloclient *client, int32_t track_id, allopicture *picture);


static inline alloclient_internal_shared *_alloclient_internal_shared_begin(alloclient *client) {
    alloclient_internal *cl = _internal(client);
    mtx_lock(&cl->shared->lock);
    return cl->shared;
}

static inline void _alloclient_internal_shared_end(alloclient *client) {
    alloclient_internal *cl = _internal(client);
    mtx_unlock(&cl->shared->lock);
}
