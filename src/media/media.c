
#include "../client/_client.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "../util.h"
#include "video/mjpeg.h"

#define DEBUG_AUDIO 0


allo_media_track *__track_find(alloclient_internal_shared *shared, uint32_t track_id) {
    for(size_t i = 0; i < shared->media_tracks.length; i++) {
        allo_media_track *track = &shared->media_tracks.data[i];
        if (track->track_id == track_id) {
            return track;
        }
    }
    return NULL;
}

void __track_create(alloclient_internal_shared *shared, uint32_t track_id, allo_media_track_type type, cJSON *comp) {
    allo_media_track_list *tracks = &shared->media_tracks;
    // reserve 1 extra and use that as our track data
    arr_reserve(tracks, tracks->length+1);
    allo_media_track *track = &tracks->data[tracks->length++];
    track->track_id = track_id;
    track->type = type;
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
        if(jformat && jformat->valuestring && strcmp(cJSON_GetStringValue(jformat), "mjpeg") == 0) {
            track->info.video.format = allo_video_format_mjpeg;
        } else {
            fprintf(stderr, "Unknown video format for track %d: %s\n", track_id, cJSON_GetStringValue(jformat));
        }
    }
}

void __track_destroy(alloclient_internal_shared *shared, allo_media_track *track) {
    if (!track) return;
    
    allo_media_track_list *tracks = &shared->media_tracks;
    
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
    for (size_t i = 0; i < tracks->length; i++) {
        if (track == &tracks->data[i]) {
            arr_splice(tracks, i, 1);
            break;
        }
    }
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
            if(track->info.video.format == allo_video_format_mjpeg) {
                pixels = allo_mjpeg_decode(data, length, &wide, &high);
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
