#include <allonet/client.h>
#include <allonet/jobs.h>
#include <opus.h>
#include <enet/enet.h>
#include "../delta.h"
#include "../assetstore.h"

typedef struct interaction_queue {
    allo_interaction *interaction;
    LIST_ENTRY(interaction_queue) pointers;
} interaction_queue;


typedef struct {
    ENetHost *host;
    ENetPeer *peer;
    OpusEncoder *opus_encoder;
    allo_client_intent *latest_intent;
    int64_t latest_intent_ts;
    int64_t latest_clockreq_ts;
    char *avatar_id;
    statehistory_t history;
    LIST_HEAD(decoder_track_list, decoder_track) decoder_tracks;
    scheduler jobs;
    assetstore assetstore;
} alloclient_internal;

typedef struct decoder_track decoder_track;

static inline alloclient_internal *_internal(alloclient *client)
{
    return (alloclient_internal*)client->_internal;
}

extern void _alloclient_parse_media(alloclient *client, unsigned char *data, int length);
extern void _alloclient_send_audio(alloclient *client, int32_t track_id, const int16_t *pcm, size_t frameCount);
extern decoder_track *_alloclient_decoder_find_for_track(alloclient *client, uint32_t track_id);
extern decoder_track *_alloclient_decoder_find_or_create_for_track(alloclient *client, uint32_t track_id);
extern void _alloclient_decoder_destroy_for_track(alloclient *client, uint32_t track_id);
