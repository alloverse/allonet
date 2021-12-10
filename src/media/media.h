#ifndef ALLONET_MEDIA_H
#define ALLONET_MEDIA_H

#include <stdio.h>
#include <opus.h>
#include <enet/enet.h>
#include "allonet/arr.h"
#include <libavcodec/avcodec.h>
#include "../threading.h"
#include <allonet/client.h>

typedef struct allo_media_subsystem allo_media_subsystem;

typedef enum {
    allo_media_type_invalid = -1,
    allo_media_type_audio,
    allo_media_type_video,

    allo_media_type_count
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

struct SwsContext;

typedef struct {
    uint32_t track_id;
    allo_media_track_type type;
    void *origin; // client that allocated the track
    arr_t(void *) recipients; // clients that want the track
    allo_media_subsystem *subsystem; // subsystem managing this track
    union {
        struct {
            OpusDecoder *decoder;
            FILE *debug;
            allo_audio_format format;
        } audio;
        struct {
            allo_video_format format;
            struct {
                AVCodec *codec;
                AVCodecContext *context;
                AVPacket *packet;
                struct SwsContext *scale_context;
            } encoder;
            struct {
                AVCodec *codec;
                AVCodecContext *context;
                struct SwsContext *scale_context;
                AVPacket *packet;
                AVFrame *frame;
            } decoder;
            int width, height;
            int framenr;
        } video;
    } info;
} allo_media_track;

typedef arr_t(allo_media_track) allo_media_track_list;
typedef struct alloclient alloclient;

typedef struct allo_media_subsystem {
    /// each subsystem is tested to see if it wants to manage this track. return true if successful.
    bool (*track_initialize)(allo_media_track *track, cJSON *component);
    void (*track_destroy)(allo_media_track *track);
    void (*parse)(alloclient *client, allo_media_track *track, unsigned char *data, size_t length, mtx_t *unlock_me);

    // only one of these should be set for a subsystem
    ENetPacket* (*create_video_packet)(allo_media_track *track, allopixel *pixels, int32_t pixels_wide, int32_t pixels_high);
    ENetPacket* (*create_audio_packet)(allo_media_track *track, const int16_t *pcm, size_t frameCount);
} allo_media_subsystem;
extern allo_media_subsystem *allo_media_subsystems[];
extern void allo_media_subsystem_register(allo_media_subsystem *subsystem);

allo_media_track_type _media_track_type_from_string(const char *string);

/// Find a track with track_id in tracklist
allo_media_track *_media_track_find(allo_media_track_list *tracklist, uint32_t track_id);

/// Create a new track in tracklist
allo_media_track *_media_track_create(allo_media_track_list *tracklist, uint32_t track_id, allo_media_track_type type);

allo_media_track *_media_track_find_or_create(allo_media_track_list *tracklist, uint32_t track_id, allo_media_track_type type);

/// Remove a track from tracklist
void _media_track_destroy(allo_media_track_list *tracklist, allo_media_track *track);

#endif
