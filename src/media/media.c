
#include "../client/_client.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "../util.h"
#include "video/mjpeg.h"
#include "audio/audio.h"
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
    
    if(track->subsystem)
        track->subsystem->track_destroy(track);
    
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

void _alloclient_media_track_find_or_create(alloclient *client, uint32_t track_id, allo_media_track_type type, const cJSON *comp) {
    alloclient_internal_shared *shared = _alloclient_internal_shared_begin(client);
    
    allo_media_track *track = _media_track_find(&shared->media_tracks, track_id);
    if (track) {
        assert(track->type == type);
    } else {
        allo_media_track *track = _media_track_create(&shared->media_tracks, track_id, type);
        int i = 0;
        allo_media_subsystem *subsystem;
        while((subsystem = allo_media_subsystems[i++])) {
            if(subsystem->track_initialize(track, comp)) {
                track->subsystem = subsystem;
                break;
            }
        }
        if(!track->subsystem) {
            const char *type = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(comp, "type"));
            const char *fmt = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(comp, "format"));
            fprintf(stderr, "allonet/media: track %d has unknown media format %s.%s, skipping.\n", track->track_id, type, fmt);
            _media_track_destroy(&shared->media_tracks, track);
        }
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

static void _alloclient_media_add_track_from_comp(alloclient *client, const cJSON *cdata)
{
    if(!cdata) return;
    cJSON *jtrack_id = cJSON_GetObjectItemCaseSensitive(cdata, "track_id");
    cJSON *jmedia_type = cJSON_GetObjectItemCaseSensitive(cdata, "type");
    allo_media_track_type type = allo_media_type_invalid;
    char *typeString = cJSON_GetStringValue(jmedia_type);
    type = _media_track_type_from_string(typeString);
    if (type == allo_media_type_invalid || !jtrack_id || !cJSON_IsNumber(jtrack_id)) {
        return;
    }
    uint32_t track_id = jtrack_id->valueint;
    _alloclient_media_track_find_or_create(client, track_id, type, cdata);
}
static void _alloclient_media_remove_track_from_comp(alloclient *client, const cJSON *cdata)
{
    if(!cdata) return;
    // if we find a decoder linked to the entity we remove it
    cJSON *track_id = cJSON_GetObjectItemCaseSensitive(cdata, "track_id");
    if(track_id) {
        fprintf(stderr, "Destroying decoder for track %d\n", track_id->valueint);
        _alloclient_media_track_destroy(client, track_id->valueint);
    }
}

void _alloclient_media_handle_statediff(alloclient *client, allo_state_diff *diff)
{
    for(size_t i = 0; i < diff->new_components.length; i++)
    {
        allo_component_ref ref = diff->new_components.data[i];
        if(strcmp(ref.name, "live_media") == 0)
        {
            _alloclient_media_add_track_from_comp(client, ref.newdata);
        }     
    }
    for(size_t i = 0; i < diff->updated_components.length; i++)
    {
        allo_component_ref ref = diff->updated_components.data[i];
        if(strcmp(ref.name, "live_media") == 0)
        {
            _alloclient_media_remove_track_from_comp(client, ref.olddata);
            _alloclient_media_add_track_from_comp(client, ref.newdata);
        }     
    }
    for(size_t i = 0; i < diff->deleted_components.length; i++)
    {
        allo_component_ref ref = diff->deleted_components.data[i];
        if(strcmp(ref.name, "live_media") == 0)
        {
            _alloclient_media_remove_track_from_comp(client, ref.olddata);
        }     
    }
}

void _alloclient_parse_media(alloclient *client, unsigned char *data, size_t length)
{
    alloclient_internal_shared *shared = _alloclient_internal_shared_begin(client);
    size_t datalength = length;
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
    bitrate_increment_received(&track->bitrates, datalength);

    track->subsystem->parse(client, track, data, length, &shared->lock);
}

allo_media_subsystem *allo_media_subsystems[255];

void allo_media_subsystem_register(allo_media_subsystem *subsystem)
{
    for(int i = 0; i < 255; i++) {
        if(allo_media_subsystems[i] == NULL) {
            allo_media_subsystems[i] = subsystem;
            return;
        }
    }
    fprintf(stderr, "allonet/media: subsystem not registered because we're out of slots\n");
}

void _allo_media_initialize(void)
{
    allo_media_audio_register();
    allo_media_mjpeg_register();
}


void allo_media_get_stats(allo_media_track_list *media_tracks, char *buffer, size_t buffersize) {
    double time = get_ts_monod();
    for(size_t i = 0; i < media_tracks->length; i++) {
        allo_media_track *track = &media_tracks->data[i];
        struct bitrate_deltas_t deltas = bitrate_deltas(&track->bitrates, time);
        
        int len = strlen(buffer);
        buffersize -= len;
        buffer += len;
        snprintf(buffer, buffersize,
                 "%s track %d\t%.3f/%.3fkb/s\n"
                 ,
                 track->type == allo_media_type_audio ? "audio" : "video",
                 track->track_id,
                 deltas.sent_bytes/1024.0,
                 deltas.received_bytes/1024.0
                 );
    }
}
