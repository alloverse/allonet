#include <allonet/client.h>
#include <allonet/jobs.h>
#include <opus.h>
#include <enet/enet.h>
#include "../delta.h"
#include <allonet/assetstore.h>

typedef struct interaction_queue {
    allo_interaction *interaction;
    LIST_ENTRY(interaction_queue) pointers;
} interaction_queue;


typedef enum {
    allo_media_type_audio,
    allo_media_type_video,
} allo_media_track_type;


typedef struct {
    uint32_t track_id;
    allo_media_track_type type;
    union {
        struct {
            OpusDecoder *decoder;
            FILE *debug;
        } audio;
        struct {
            int empty_structs_not_allowed_placeholder;
        } video;
    } info;
} allo_media_track;

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
    assetstore assetstore; // asset state tracking
    arr_t(allo_media_track) media_tracks;
} alloclient_internal;

static inline alloclient_internal *_internal(alloclient *client)
{
    return (alloclient_internal*)client->_internal;
}

extern allo_media_track *_allo_media_track_create(alloclient *client, uint32_t track_id, allo_media_track_type type);
extern allo_media_track *_allo_media_track_find(alloclient *client, uint32_t track_id, allo_media_track_type type);
extern void _alloclient_media_destroy(alloclient *client, uint32_t track_id);
extern void _alloclient_parse_media(alloclient *client, unsigned char *data, size_t length);
extern void _alloclient_send_audio(alloclient *client, int32_t track_id, const int16_t *pcm, size_t frameCount);
