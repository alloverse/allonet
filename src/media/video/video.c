#include "../../client/_client.h"
#include "../../util.h"
#include "mjpeg.h"
#include <string.h>
#include <assert.h>
#include <x264/x264.h>

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
    
    alloclient_internal_shared *shared = _alloclient_internal_shared_begin(client);
    allo_media_track *track = _media_track_find(&shared->media_tracks, track_id);
    
    if (track == NULL) {
        fprintf(stderr, "alloclient: Skipping send video as track is not allocated\n");
        goto end;
    }
    
    if (track->type != allo_media_type_video) {
        fprintf(stderr, "alloclient: Skipping send video as track is not for video\n");
        goto end;
    }
    
    ENetPacket *packet = NULL;
    if (track->info.video.format == allo_video_format_mjpeg) {
        allo_mjpeg_encode(pixels, pixels_wide, pixels_high, (allo_mjpeg_encode_func*)create_packet, &packet);
    } else if (track->info.video.format == allo_video_format_h264) {
        track->info.video.x264.pic_in->i_pts = track->info.video.x264.i_frame++;
        int frame_size = x264_encoder_encode(
            track->info.video.x264.encoder,
            &track->info.video.x264.nal,
            &track->info.video.x264.i_nal,
            track->info.video.x264.pic_in,
            track->info.video.x264.pic_out
        );
        
        if( frame_size < 0 ) {
            fprintf(stderr, "alloclient: Something went wrong in the h264 encoding\n");
            goto end;
        }
        
        packet = enet_packet_create(NULL, frame_size + 4, 0);
        memcpy(packet->data + 4, track->info.video.x264.nal->p_payload, frame_size);
    }
    
    const int headerlen = sizeof(int32_t); // track id header
    int32_t big_track_id = htonl(track_id);
    memcpy(packet->data, &big_track_id, headerlen);

    int ok = enet_peer_send(_internal(client)->peer, CHANNEL_MEDIA, packet);
    allo_statistics.bytes_sent[0] += packet->dataLength;
    allo_statistics.bytes_sent[1+CHANNEL_MEDIA] += packet->dataLength;
    assert(ok == 0);
    
end:
    _alloclient_internal_shared_end(client);
}
