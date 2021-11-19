
#include "../client/_client.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "../util.h"
#include "video/mjpeg.h"
#include <libavcodec/avcodec.h>

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
    memset(track, 0, sizeof(allo_media_track));
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
    
    allo_media_subsystems[track->type].track_destroy(track);
    
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

void _alloclient_media_track_find_or_create(alloclient *client, uint32_t track_id, allo_media_track_type type, cJSON *comp) {
    alloclient_internal_shared *shared = _alloclient_internal_shared_begin(client);
    
    allo_media_track *track = _media_track_find(&shared->media_tracks, track_id);
    if (track) {
        assert(track->type == type);
    } else {
        allo_media_track *track = _media_track_create(&shared->media_tracks, track_id, type);
        allo_media_subsystems[type].track_initialize(track, comp);
    }
    
    _alloclient_internal_shared_end(client);
}


void _alloclient_media_track_destroy(alloclient *client, uint32_t track_id)
{
    alloclient_internal_shared *shared = _alloclient_internal_shared_begin(client);
    
    allo_media_track_list *tracks = &shared->media_tracks;
    allo_media_track *track = _media_track_find(&shared->media_tracks, track_id);
    
    if (track) {
        _media_track_destroy(&shared->media_tracks, track);
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
    allo_media_track *track = _media_track_find(&shared->media_tracks, track_id);
    if (!track) {
        // it is possible to receive data before the track exists in components
        _alloclient_internal_shared_end(client);
        return;
    }
    
    allo_media_subsystems[track->type].parse(client, track, data, length, &shared->lock);
}

allo_media_subsystem allo_media_subsystems[allo_media_type_count];

void _allo_media_initialize(void)
{
    allo_media_subsystems[allo_media_type_audio] = allo_audio_subsystem;
    allo_media_subsystems[allo_media_type_video] = allo_video_subsystem;
}
