#include "_client.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "../util.h"

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

void __track_create(alloclient_internal_shared *shared, uint32_t track_id, allo_media_track_type type) {
    allo_media_track_list *tracks = &shared->media_tracks;
    // reserve 1 extra and use that as our track data
    arr_reserve(tracks, tracks->length+1);
    allo_media_track *track = &tracks->data[tracks->length++];
    track->track_id = track_id;
    track->type = type;
    if (type == allo_media_type_audio) {
        int err;
        track->info.audio.decoder = opus_decoder_create(48000, 1, &err);
        if (DEBUG_AUDIO) {
            char name[255]; snprintf(name, 254, "track_%04d.pcm", track_id);
            track->info.audio.debug = fopen(name, "wb");
            fprintf(stderr, "Opening decoder for %s\n", name);
        } else {
            track->info.audio.debug = NULL;
        }
        assert(track->info.audio.decoder);
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


extern void _alloclient_media_track_find_or_create(alloclient *client, uint32_t track_id, allo_media_track_type type) {
    alloclient_internal_shared *shared = _alloclient_internal_shared_begin(client);
    
    allo_media_track *track = __track_find(shared, track_id);
    if (track) {
        assert(track->type == type);
    } else {
        __track_create(shared, track_id, type);
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
    } else {
        
    }
    
    _alloclient_internal_shared_end(client);
}

void _alloclient_send_audio(alloclient *client, int32_t track_id, const int16_t *pcm, size_t frameCount)
{
    assert(frameCount == 480 || frameCount == 960);
    
    if (_internal(client)->peer == NULL) {
        fprintf(stderr, "alloclient: Skipping send audio as we don't even have a peer\n");
        return;
    }
    
    if (_internal(client)->peer->state != ENET_PEER_STATE_CONNECTED) {
        fprintf(stderr, "alloclient: Skipping send audio as peer is not connected\n");
        return;
    }
    
    const int headerlen = sizeof(int32_t); // track id header
    const int outlen = headerlen + frameCount*2 + 1; // theoretical max
    ENetPacket *packet = enet_packet_create(NULL, outlen, 0 /* unreliable */);
    assert(packet != NULL);
    int32_t big_track_id = htonl(track_id);
    memcpy(packet->data, &big_track_id, headerlen);

    int len = opus_encode (
        _internal(client)->opus_encoder, 
        pcm, frameCount,
        packet->data + headerlen, outlen - headerlen
    );

    if (len < 3) {  // error or DTX ("do not transmit")
        enet_packet_destroy(packet);
        if (len < 0) {
            fprintf(stderr, "Error encoding audio to send: %d", len);
        }
        return;
    }
    // +1 because stupid server code assumes all packets end with a newline... FIX THE DAMN PROTOCOL
    int ok = enet_packet_resize(packet, headerlen + len+1);
    assert(ok == 0); (void)ok;
    ok = enet_peer_send(_internal(client)->peer, CHANNEL_MEDIA, packet);
    allo_statistics.bytes_sent[0] += packet->dataLength;
    allo_statistics.bytes_sent[1+CHANNEL_MEDIA] += packet->dataLength;
    assert(ok == 0);
}
