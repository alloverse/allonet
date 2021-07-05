#include "../../client/_client.h"
#include "../../util.h"
#include "mjpeg.h"
#include <string.h>
#include <assert.h>

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

static inline void rgb32_to_yuv420p(uint8_t *rgb, uint8_t* y, uint8_t* u, uint8_t* v, uint32_t width, uint32_t height) {
    for (uint32_t j = 0; j < height; j++) {
        for (uint32_t k = 0; k < width; k++) {
            uint8_t sR = (uint8_t)(rgb[0]);
            uint8_t sG = (uint8_t)(rgb[1]);
            uint8_t sB = (uint8_t)(rgb[2]);
            
            *y = (uint8_t)((66 * sR + 129 * sG + 25 * sB + 128) >> 8) + 16;
            
            if (0 == j % 2 && 0 == k % 2) {
                *u = (uint8_t)((-38 * sR - 74 * sG + 112 * sB + 128) >> 8) + 128;
                *v = (uint8_t)((112 * sR - 94 * sG - 18 * sB + 128) >> 8) + 128;
                
                u++;
                v++;
            }
            y++;
            rgb += 4; // 4 bytes
        }
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
        
        if (track->info.video.encoder.codec == NULL) {
            track->info.video.encoder.codec = avcodec_find_encoder(AV_CODEC_ID_H264);
            if (track->info.video.encoder.codec == NULL) {
                fprintf(stderr, "No encoder\n");
            }
            track->info.video.encoder.context = avcodec_alloc_context3(track->info.video.encoder.codec);
            track->info.video.picture = av_frame_alloc();
            
            track->info.video.encoder.context->width = track->info.video.width;
            track->info.video.encoder.context->height = track->info.video.height;
            int ret = avcodec_open2(track->info.video.encoder.context, track->info.video.encoder.codec, NULL);
            if (ret != 0) {
                fprintf(stderr, "avcodec_open2 return %d when opening encoder\n", ret);
            }
        }
        
        AVFrame *frame = av_frame_alloc();
        frame->format = AV_PIX_FMT_RGBA;
        frame->width = track->info.video.width;
        frame->height = track->info.video.height;
        
        assert(av_frame_get_buffer(frame, 0));
        assert(av_frame_make_writable(frame));
        memcpy(frame->data[0], pixels, frame->width * frame->height * sizeof(allopixel));
        
        assert(avcodec_send_frame(track->info.video.encoder.context, frame));
        
        AVPacket avpacket;
        av_init_packet(&avpacket);
        int ret = 0;
        // TODO: Might need to loop and receive multiple packets (because encoded frames can come out of order)
        ret = avcodec_receive_packet(track->info.video.encoder.context, &avpacket);
        if (ret >= 0) {
            packet = enet_packet_create(NULL, avpacket.size + 4, 0);
            memcpy(packet->data + 4, avpacket.data, avpacket.size);
        } else {
            fprintf(stderr, "alloclient: Something went wrong in the h264 encoding\n");
        }
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
