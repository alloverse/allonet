
#include "../client/_client.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "../util.h"
#include "video/mjpeg.h"
#include <x264/x264.h>
#include <libavcodec/decode.h>

#define DEBUG_AUDIO 0


allo_media_track *_media_track_find(allo_media_track_list *tracklist, uint32_t track_id) {
    for(size_t i = 0; i < tracklist->length; i++) {
        allo_media_track *track = &tracklist->data[i];
        if (track->track_id == track_id) {
            return track;
        }
    }
    return NULL;
}

allo_media_track *_media_track_create(allo_media_track_list *tracklist, uint32_t track_id, allo_media_track_type type) {
    // reserve 1 extra and use that as our track data
    arr_reserve(tracklist, tracklist->length + 1);
    allo_media_track *track = &tracklist->data[tracklist->length++];
    track->track_id = track_id;
    track->type = type;
    arr_init(&track->recipients);
    return track;
}

allo_media_track *_media_track_find_or_create(allo_media_track_list *tracklist, uint32_t track_id, allo_media_track_type type) {
    allo_media_track *track = _media_track_find(tracklist, track_id);
    if (track) {
        return track;
    } else {
        return _media_track_create(tracklist, track_id, type);
    }
}

void _media_track_destroy(allo_media_track_list *tracklist, allo_media_track *track) {
    if (!track) return;
    
    if (track->type == allo_media_type_audio) {
        assert(track->info.audio.decoder);

        if (DEBUG_AUDIO) {
            char name[255]; snprintf(name, 254, "track_%04d.pcm", track->track_id);
            fprintf(stderr, "Closing decoder for %s\n", name);
            if (track->info.audio.debug) {
                fclose(track->info.audio.debug);
                track->info.audio.debug = NULL;
            }
        }
        opus_decoder_destroy(track->info.audio.decoder);
        track->info.audio.decoder = NULL;
    } else {
        //TODO: video stuff
    }
    
    // remove from the list
    for (size_t i = 0; i < tracklist->length; i++) {
        if (track == &tracklist->data[i]) {
            arr_splice(tracklist, i, 1);
            break;
        }
    }
}

allo_media_track_type _media_track_type_from_string(const char *string) {
    if (string == NULL) return allo_media_type_invalid;
    if (strcmp("video", string) == 0) {
        return allo_media_type_video;
    } else if (strcmp("audio", string) == 0) {
        return allo_media_type_audio;
    } else {
        fprintf(stderr, "skipping unknown media track type %s\n", string);
        return allo_media_type_invalid;
    }
}


static inline allo_media_track *__track_find(alloclient_internal_shared *shared, uint32_t track_id) {
    return _media_track_find(&shared->media_tracks, track_id);
}

static inline void __track_create(alloclient_internal_shared *shared, uint32_t track_id, allo_media_track_type type, cJSON *comp) {
    allo_media_track *track = _media_track_create(&shared->media_tracks, track_id, type);
    cJSON *jformat = cJSON_GetObjectItemCaseSensitive(comp, "format");
    if (type == allo_media_type_audio) {
        int err;
        track->info.audio.format = allo_audio_format_opus;
        track->info.audio.decoder = opus_decoder_create(48000, 1, &err);
        if (DEBUG_AUDIO) {
            char name[255]; snprintf(name, 254, "track_%04d.pcm", track_id);
            track->info.audio.debug = fopen(name, "wb");
            fprintf(stderr, "Opening decoder for %s\n", name);
        } else {
            track->info.audio.debug = NULL;
        }
        assert(track->info.audio.decoder);
    } else if(type == allo_media_type_video) {
        if (jformat == NULL || jformat->valuestring == NULL) {
            
        }
        if (strcmp(cJSON_GetStringValue(jformat), "mjpeg") == 0) {
            track->info.video.format = allo_video_format_mjpeg;
        } else if (strcmp(cJSON_GetStringValue(jformat), "h264") == 0) {
            cJSON *jmeta = cJSON_GetObjectItemCaseSensitive(comp, "metadata");
            cJSON *jwidth = cJSON_GetObjectItemCaseSensitive(jmeta, "width");
            cJSON *jheight = cJSON_GetObjectItemCaseSensitive(jmeta, "height");
            
            track->info.video.format = allo_video_format_h264;
            track->info.video.x264.params = malloc(sizeof(x264_param_t));
            x264_param_t *param = track->info.video.x264.params;
            /* Get default params for preset/tuning */
            x264_param_default_preset( param, "medium", NULL );
            /* Configure non-default params */
            param->i_bitdepth = 8;
            param->i_csp = X264_CSP_BGRA;
            param->i_width  = jwidth->valueint;
            param->i_height = jheight->valueint;
            param->b_vfr_input = 0;
            param->b_repeat_headers = 1;
            param->b_annexb = 1;
            
            track->info.video.x264.encoder = x264_encoder_open(param);
            track->info.video.x264.nal = NULL;
            track->info.video.x264.pic_in = malloc(sizeof(x264_picture_t));
            track->info.video.x264.pic_out = malloc(sizeof(x264_picture_t));
            track->info.video.x264.i_nal = 0;
            track->info.video.x264.i_frame = 0;
            
            x264_param_apply_profile( param, "high" );
            
            x264_picture_alloc(track->info.video.x264.pic_in, param->i_csp, param->i_width, param->i_height);
            
            avcodec_register_all();
            
            track->info.video.libav.codec = avcodec_find_decoder(AV_CODEC_ID_H264);
            if (track->info.video.libav.codec == NULL) {
                fprintf(stderr, "No codec\n");
            }
            track->info.video.libav.context = avcodec_alloc_context3(track->info.video.libav.codec);
            track->info.video.libav.picture = av_frame_alloc();
            
            track->info.video.libav.context->width = jwidth->valueint;
            track->info.video.libav.context->height = jheight->valueint;
            int ret = avcodec_open2(track->info.video.libav.context, track->info.video.libav.codec, NULL);
            if (ret != 0) {
                fprintf(stderr, "avcodec_open2 return %d\n", ret);
            }
            
        } else {
            fprintf(stderr, "Unknown video format for track %d: %s\n", track_id, cJSON_GetStringValue(jformat));
        }
    }
}

static inline void __track_destroy(alloclient_internal_shared *shared, allo_media_track *track) {
    if (track->type == allo_media_type_video) {
        if (track->info.video.format == allo_video_format_h264 && track->info.video.x264.encoder) {
            x264_encoder_close(track->info.video.x264.encoder);
            x264_picture_clean(track->info.video.x264.pic_in);
            
            free(track->info.video.x264.pic_in);
            free(track->info.video.x264.pic_out);
            free(track->info.video.x264.params);
            track->info.video.x264.encoder = NULL;
        }
    }
    _media_track_destroy(&shared->media_tracks, track);
}


void _alloclient_media_track_find_or_create(alloclient *client, uint32_t track_id, allo_media_track_type type, cJSON *comp) {
    alloclient_internal_shared *shared = _alloclient_internal_shared_begin(client);
    
    allo_media_track *track = __track_find(shared, track_id);
    if (track) {
        assert(track->type == type);
    } else {
        __track_create(shared, track_id, type, comp);
    }
    
    _alloclient_internal_shared_end(client);
}


void _alloclient_media_track_destroy(alloclient *client, uint32_t track_id)
{
    alloclient_internal_shared *shared = _alloclient_internal_shared_begin(client);
    
    allo_media_track_list *tracks = &shared->media_tracks;
    allo_media_track *track = __track_find(shared, track_id);
    
    if (track) {
        __track_destroy(shared, track);
    } else {
        fprintf(stderr, "A decoder for track_id %d was not found\n", track_id);
        fprintf(stderr, "Active decoder tracks:\n ");
        for (size_t i = 0; i < tracks->length; i++) {
            allo_media_track *track = &tracks->data[i];
            fprintf(stderr, "%d, ", track->track_id);
        }
    }
    
    _alloclient_internal_shared_end(client);
}

static void yuv2rgb(uint8_t yValue, uint8_t uValue, uint8_t vValue,
        uint8_t *r, uint8_t *g, uint8_t *b) {
    int rTmp = yValue + (1.370705 * (vValue-128));
    // or fast integer computing with a small approximation
    // rTmp = yValue + (351*(vValue-128))>>8;
    int gTmp = yValue - (0.698001 * (vValue-128)) - (0.337633 * (uValue-128));
    // gTmp = yValue - (179*(vValue-128) + 86*(uValue-128))>>8;
    int bTmp = yValue + (1.732446 * (uValue-128));
//    int bTmp = yValue + (443*(uValue-128))>>8;
    *r = MIN(MAX(rTmp, 0), 255);
    *g = MIN(MAX(gTmp, 0), 255);
    *b = MIN(MAX(bTmp, 0), 255);
}

static inline void yuv420p_to_rgb32(uint8_t* y, uint8_t* u, uint8_t* v, uint8_t *rgb, uint32_t width, uint32_t height) {
    for (uint32_t j = 0; j < height; j++) {
        for (uint32_t k = 0; k < width; k++) {
            yuv2rgb(*y, *u, *v, &rgb[0], &rgb[1], &rgb[2]);
            rgb[3] = 255;
            rgb += 4;
            y++;u++;v++;
        }
    }
}

void _alloclient_parse_media(alloclient *client, unsigned char *data, size_t length)
{
    alloclient_internal_shared *shared = _alloclient_internal_shared_begin(client);
    
    // get the track_id from the top of data
    uint32_t track_id;
    assert(length >= sizeof(track_id) + 3);
    memcpy(&track_id, data, sizeof(track_id));
    track_id = ntohl(track_id);
    data += sizeof(track_id);
    length -= sizeof(track_id);
    
    // see if we have allocated a media object for this
    allo_media_track *track = __track_find(shared, track_id);
    if (!track) {
        // it is possible to receive data before the track exists in components
        _alloclient_internal_shared_end(client);
        return;
    }
    
    if (track->type == allo_media_type_audio) {
        // todo: decode on another tread
        OpusDecoder *decoder = track->info.audio.decoder;
        FILE *debugFile = track->info.audio.debug;
        
        const int maximumFrameCount = 5760; // 120ms as per documentation
        int16_t *pcm = calloc(maximumFrameCount, sizeof(int16_t));
        int samples_decoded = opus_decode(decoder, (unsigned char*)data, length, pcm, maximumFrameCount, 0);

        assert(samples_decoded >= 0);
        if (debugFile) {
            fwrite(pcm, sizeof(int16_t), samples_decoded, debugFile);
            fflush(debugFile);
        }

        _alloclient_internal_shared_end(client);
        
        if(!client->audio_callback || client->audio_callback(client, track_id, pcm, samples_decoded)) {
            free(pcm);
        }
        return;
    } else if (track->type == allo_media_type_video) {
        int32_t wide, high;
        if (client->video_callback) {
            allopixel *pixels = NULL;
            if (track->info.video.format == allo_video_format_mjpeg) {
                pixels = allo_mjpeg_decode(data, length, &wide, &high);
            } else if (track->info.video.format == allo_video_format_h264) {
                AVPacket avpacket;
                avpacket.size = length;
                avpacket.data = data;
                int ret = avcodec_send_packet(track->info.video.libav.context, &avpacket);
                
                if (ret != 0) {
                    fprintf(stderr, "avcodec_send_packet return %d\n", ret);
                }
                ret = avcodec_receive_frame(track->info.video.libav.context, track->info.video.libav.picture);
                if (ret != 0) {
                    fprintf(stderr, "avcodec_receive_frame return %d\n", ret);
                }
                
                pixels = malloc(track->info.video.libav.picture->width * track->info.video.libav.picture->height * sizeof(allopixel));
//                for (int h = 0; h < track->info.video.libav.picture->height; h++) {
//                    for (int w = 0; w < track->info.video.libav.picture->width; w++) {
//                        int i = h*track->info.video.libav.picture->width + w;
//                        allopixel *p = &pixels[i];
//                        p->r = track->info.video.libav.picture->data[0][i];
//                        p->g = track->info.video.libav.picture->data[1][i];
//                        p->b = track->info.video.libav.picture->data[2][i];
//                        p->a = 255;
//                    }
//                }
                yuv420p_to_rgb32(
                    track->info.video.libav.picture->data[0],
                    track->info.video.libav.picture->data[1],
                    track->info.video.libav.picture->data[2],
                    (uint8_t*)pixels,
                    track->info.video.libav.picture->width,
                    track->info.video.libav.picture->height
                );
            }
            _alloclient_internal_shared_end(client);
            
            if (pixels && client->video_callback(client, track_id, pixels, wide, high)) {
                free(pixels);
            }
            return;
        }
    }
    
    _alloclient_internal_shared_end(client);
}
