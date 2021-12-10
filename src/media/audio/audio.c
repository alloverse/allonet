#include "../../client/_client.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "../../util.h"
#include "../video/mjpeg.h"

#define DEBUG_AUDIO 0

static bool audio_track_initialize(allo_media_track *track, cJSON *component)
{
    if(track->type != allo_media_type_audio) return false;

    int err;
    cJSON *jformat = cJSON_GetObjectItemCaseSensitive(component, "format");
    if(jformat && jformat->valuestring && strcmp(cJSON_GetStringValue(jformat), "opus") == 0) {
        track->info.audio.format = allo_audio_format_opus;
        track->info.audio.decoder = opus_decoder_create(48000, 1, &err);
    } else {
        fprintf(stderr, "Unknown audio format for track %d: %s\n", track->track_id, cJSON_GetStringValue(jformat));
        return false;
    }
    if (DEBUG_AUDIO) {
        char name[255]; snprintf(name, 254, "track_%04d.pcm", track->track_id);
        track->info.audio.debug = fopen(name, "wb");
        fprintf(stderr, "Opening audio decoder for %s\n", name);
    } else {
        track->info.audio.debug = NULL;
    }
    
    return true;
}

static void audio_track_destroy(allo_media_track *track)
{
    assert(track->info.audio.decoder);

    if (DEBUG_AUDIO) {
        char name[255]; snprintf(name, 254, "track_%04d.pcm", track->track_id);
        fprintf(stderr, "Closing audio decoder for %s\n", name);
        if (track->info.audio.debug) {
            fclose(track->info.audio.debug);
            track->info.audio.debug = NULL;
        }
    }
    opus_decoder_destroy(track->info.audio.decoder);
    track->info.audio.decoder = NULL;
}

static void parse_audio(alloclient *client, allo_media_track *track, unsigned char *mediadata, size_t length, mtx_t *unlock_me)
{    
    uint32_t track_id = track->track_id;
    OpusDecoder *decoder = track->info.audio.decoder;
    FILE *debugFile = track->info.audio.debug;
    
    const int maximumFrameCount = 5760; // 120ms as per documentation
    int16_t *pcm = calloc(maximumFrameCount, sizeof(int16_t));
    int samples_decoded = opus_decode(decoder, (unsigned char*)mediadata, length, pcm, maximumFrameCount, 0);

    assert(samples_decoded >= 0);
    if (debugFile) {
        fwrite(pcm, sizeof(int16_t), samples_decoded, debugFile);
        fflush(debugFile);
    }

    mtx_unlock(unlock_me);
    
    if(!client->audio_callback || client->audio_callback(client, track_id, pcm, samples_decoded)) {
        free(pcm);
    }
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

void allo_media_audio_register(void)
{
    allo_media_subsystem *sys = malloc(sizeof(allo_media_subsystem));
    sys->parse = parse_audio;
    sys->track_initialize = audio_track_initialize;
    sys->track_destroy = audio_track_destroy;
    allo_media_subsystem_register(sys);
    printf("allonet: initialized audio media subsystem\n");
}
