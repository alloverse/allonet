#ifndef ALLONET_MEDIA_H
#define ALLONET_MEDIA_H

#include <stdio.h>
#include <opus.h>
#include <enet/enet.h>
#include "allonet/arr.h"
#include <libavcodec/decode.h>

typedef enum {
    allo_media_type_invalid = -1,
    allo_media_type_audio,
    allo_media_type_video,
} allo_media_track_type;

typedef enum allo_audio_format {
    allo_audio_format_invalid = -1,
    allo_audio_format_opus,
} allo_audio_format;

typedef enum allo_video_format {
    allo_video_format_invalid = -1,
    allo_video_format_mjpeg,
    allo_video_format_h264
} allo_video_format;


typedef struct {
    uint32_t track_id;
    allo_media_track_type type;
    void *origin; // client that allocated the track
    arr_t(void *) recipients; // clients that want the track
    union {
        struct {
            OpusDecoder *decoder;
            FILE *debug;
            allo_audio_format format;
        } audio;
        struct {
            allo_video_format format;
            struct {
                struct x264_t *encoder;
                struct x264_param_t *params;
                struct x264_nal_t *nal;
                struct x264_picture_t *pic_in;
                struct x264_picture_t *pic_out;
                int i_nal;
                int i_frame;
            } x264;
            struct {
                AVCodec *codec;
                AVCodecContext *context;
                int frame, len;
                AVFrame *picture;
                
            } libav;
        } video;
    } info;
} allo_media_track;

typedef arr_t(allo_media_track) allo_media_track_list;


allo_media_track_type _media_track_type_from_string(const char *string);

/// Find a track with track_id in tracklist
allo_media_track *_media_track_find(allo_media_track_list *tracklist, uint32_t track_id);

/// Create a new track in tracklist
allo_media_track *_media_track_create(allo_media_track_list *tracklist, uint32_t track_id, allo_media_track_type type);

allo_media_track *_media_track_find_or_create(allo_media_track_list *tracklist, uint32_t track_id, allo_media_track_type type);

/// Remove a track from tracklist
void _media_track_destroy(allo_media_track_list *tracklist, allo_media_track *track);

#endif
