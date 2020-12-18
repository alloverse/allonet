#include "_client.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "../util.h"

#define DEBUG_AUDIO 0

typedef struct decoder_track {
    OpusDecoder *decoder;
    uint32_t track_id;
    FILE* debug;
    LIST_ENTRY(decoder_track) pointers;
} decoder_track;


void _alloclient_parse_media(alloclient *client, unsigned char *data, int length)
{
    uint32_t track_id;
    assert(length >= sizeof(track_id) + 3);
    memcpy(&track_id, data, sizeof(track_id));
    track_id = ntohl(track_id);
    data += sizeof(track_id);
    length -= sizeof(track_id);

    // todo: decode on another tread
    decoder_track *dec = _alloclient_decoder_find_or_create_for_track(client, track_id);
    const int maximumFrameCount = 5760; // 120ms as per documentation
    int16_t *pcm = calloc(maximumFrameCount, sizeof(int16_t));
    int samples_decoded = opus_decode(dec->decoder, (unsigned char*)data, length, pcm, maximumFrameCount, 0);

    assert(samples_decoded >= 0);
    if (DEBUG_AUDIO && dec->debug) {
        fwrite(pcm, sizeof(int16_t), samples_decoded, dec->debug);
        fflush(dec->debug);
    }

    if(!client->audio_callback || client->audio_callback(client, track_id, pcm, samples_decoded))
    {
        free(pcm);
    }
}


/// Find by track_id, else NULL
decoder_track *_alloclient_decoder_find_for_track(alloclient *client, uint32_t track_id)
{
    decoder_track *dec = NULL;
    LIST_FOREACH(dec, &_internal(client)->decoder_tracks, pointers)
    {
        if(dec->track_id == track_id)
        {
            return dec;
        }
    }
    return dec;
}

decoder_track *_alloclient_decoder_find_or_create_for_track(alloclient *client, uint32_t track_id)
{
    decoder_track *dec = _alloclient_decoder_find_for_track(client, track_id);
    if (dec) { return dec; }
    
    dec = (decoder_track*)calloc(1, sizeof(decoder_track));
    dec->track_id = track_id;
    int err;
    dec->decoder = opus_decoder_create(48000, 1, &err);
    if (DEBUG_AUDIO) {
        char name[255]; snprintf(name, 254, "track_%04d.pcm", track_id);
        dec->debug = fopen(name, "wb");
        fprintf(stderr, "Opening decoder for %s\n", name);
    }
    assert(dec->decoder);
    LIST_INSERT_HEAD(&_internal(client)->decoder_tracks, dec, pointers);

    return dec;
}

void _alloclient_decoder_destroy_for_track(alloclient *client, uint32_t track_id)
{
    decoder_track *dec = _alloclient_decoder_find_for_track(client, track_id);
    if (dec == NULL) {
        fprintf(stderr, "A decoder for track_id %d was not found\n", track_id);
        fprintf(stderr, "Active decoder tracks:\n ");
        dec = NULL;
        LIST_FOREACH(dec, &_internal(client)->decoder_tracks, pointers)
        {
            fprintf(stderr, "%d, ", dec->track_id);
        }

        return;
    }
    assert(dec->decoder);

    if (DEBUG_AUDIO) {
        char name[255]; snprintf(name, 254, "track_%04d.pcm", track_id);
        fprintf(stderr, "Closing decoder for %s\n", name);
        if (dec->debug) {
            fclose(dec->debug);
            dec->debug = NULL;
        }
    }
    dec->track_id = -1;
    opus_decoder_destroy(dec->decoder);
    dec->decoder = NULL;
    LIST_REMOVE(dec, pointers);

    free(dec);
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
    assert(ok == 0);
    ok = enet_peer_send(_internal(client)->peer, CHANNEL_MEDIA, packet);
    allo_statistics.bytes_sent[0] += packet->dataLength;
    allo_statistics.bytes_sent[1+CHANNEL_MEDIA] += packet->dataLength;
    assert(ok == 0);
}
