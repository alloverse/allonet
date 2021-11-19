#include "../../client/_client.h"
#include "../../util.h"
#include "h264.h"
#include "mjpeg.h"
#include <string.h>
#include <assert.h>


static void video_track_initialize(allo_media_track *track, cJSON *comp)
{
    cJSON *jformat = cJSON_GetObjectItemCaseSensitive(comp, "format");
    if (strcmp(cJSON_GetStringValue(jformat), "mjpeg") == 0) {
        track->info.video.format = allo_video_format_mjpeg;
    } else if (strcmp(cJSON_GetStringValue(jformat), "h264") == 0) {
        cJSON *jmeta = cJSON_GetObjectItemCaseSensitive(comp, "metadata");
        cJSON *jwidth = cJSON_GetObjectItemCaseSensitive(jmeta, "width");
        cJSON *jheight = cJSON_GetObjectItemCaseSensitive(jmeta, "height");
        
        avcodec_register_all();
        
        track->info.video.format = allo_video_format_h264;
        // Let's not create encoder/decoder until they are used
        track->info.video.width = jwidth->valueint;
        track->info.video.height = jheight->valueint;
    } else {
        track->info.video.format = allo_video_format_invalid;
        fprintf(stderr, "Unknown video format for track %d: %s\n", track->track_id, cJSON_GetStringValue(jformat));
    }
}

static void video_track_destroy(allo_media_track *track)
{
    if (track->info.video.format == allo_video_format_h264 && track->info.video.decoder.codec) {
        // TODO: cleanup libav decoder
    }
}

static void parse_video(alloclient *client, allo_media_track *track, unsigned char *mediadata, size_t length, mtx_t *unlock_me)
{
    uint32_t track_id = track->track_id;
    int32_t wide, high;
    if (!client->video_callback) {
        mtx_unlock(unlock_me);
        return;
    }

    allopixel *pixels = NULL;
    if(track->info.video.format == allo_video_format_mjpeg) {
        pixels = allo_mjpeg_decode(mediadata, length, &wide, &high);
    } else if (track->info.video.format == allo_video_format_h264) {
        pixels = allo_video_parse_h264(client, track, mediadata, length);
    }
    mtx_unlock(unlock_me);
    
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
        packet = allo_video_write_h264(track, pixels, pixels_wide, pixels_high);
    }
    
    if(packet) {
        const int headerlen = sizeof(int32_t); // track id header
        int32_t big_track_id = htonl(track_id);
        memcpy(packet->data, &big_track_id, headerlen);

        int ok = enet_peer_send(_internal(client)->peer, CHANNEL_MEDIA, packet); (void)ok;
        allo_statistics.bytes_sent[0] += packet->dataLength;
        allo_statistics.bytes_sent[1+CHANNEL_MEDIA] += packet->dataLength;
        assert(ok == 0);
    }
end:
    _alloclient_internal_shared_end(client);
}

allo_media_subsystem allo_video_subsystem =
{
    .parse = parse_video,
    .track_initialize = video_track_initialize,
    .track_destroy = video_track_destroy,
};