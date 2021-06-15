#include "../../client/_client.h"
#include "../../util.h"
#include "mjpeg.h"
#include <string.h>
#include <assert.h>

static void audio_track_initialize(allo_media_track *track, cJSON *component)
{

}

static void audio_track_destroy(allo_media_track *track)
{

}

static void parse_video(alloclient *client, allo_media_track *track, unsigned char *mediadata, size_t length)
{
    uint32_t track_id = track->track_id;
    int32_t wide, high;
    if (!client->video_callback) {
        _alloclient_internal_shared_end(client);
        return;
    }

    allopixel *pixels = NULL;
    if(track->info.video.format == allo_video_format_mjpeg) {
        pixels = allo_mjpeg_decode(mediadata, length, &wide, &high);
    }
    _alloclient_internal_shared_end(client);
    
    if (pixels && client->video_callback(client, track_id, pixels, wide, high)) {
        free(pixels);
    }
}

static void create_packet(ENetPacket **packet, void* encoded, int size)
{
    if (*packet) {
        size_t acc = (*packet)->dataLength;
        enet_packet_resize(*packet, acc + size);
        memcpy((*packet)->data + acc, encoded, size);
    } else {
        *packet = enet_packet_create(NULL, size + 4, 0 /* unreliable */);
        memcpy((*packet)->data + 4, encoded, size);
    }
}

void _alloclient_send_video(alloclient *client, int32_t track_id, allopixel *pixels, int32_t pixels_wide, int32_t pixels_high)
{
    if (_internal(client)->peer == NULL) {
        fprintf(stderr, "alloclient: Skipping send video as we don't even have a peer\n");
        return;
    }
    
    if (_internal(client)->peer->state != ENET_PEER_STATE_CONNECTED) {
        fprintf(stderr, "alloclient: Skipping send video as peer is not connected\n");
        return;
    }

    ENetPacket *packet = NULL;
    allo_mjpeg_encode(pixels, pixels_wide, pixels_high, (allo_mjpeg_encode_func*)create_packet, &packet);
    
    const int headerlen = sizeof(int32_t); // track id header
    int32_t big_track_id = htonl(track_id);
    memcpy(packet->data, &big_track_id, headerlen);

    int ok = enet_peer_send(_internal(client)->peer, CHANNEL_MEDIA, packet); (void)ok;
    allo_statistics.bytes_sent[0] += packet->dataLength;
    allo_statistics.bytes_sent[1+CHANNEL_MEDIA] += packet->dataLength;
    assert(ok == 0);
}

allo_media_subsystem allo_video_subsystem =
{
    .parse = parse_video,
    .track_initialize = video_track_initialize,
    .track_destroy = video_track_destroy,
}