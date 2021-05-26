#include "_client.h"
#include "../util.h"

static void create_packet(ENetPacket **packet, void* data, int size)
{
    *packet = enet_packet_create(NULL, size + 4, 0 /* unreliable */);
    memcpy((*packet)->data + 4, &data, size);
}

void alloclient_send_video(alloclient *client, int32_t track_id, allopixel *pixels, int32_t pixels_wide, int32_t pixels_high)
{
    if (_internal(client)->peer == NULL) {
        fprintf(stderr, "alloclient: Skipping send audio as we don't even have a peer\n");
        return;
    }
    
    if (_internal(client)->peer->state != ENET_PEER_STATE_CONNECTED) {
        fprintf(stderr, "alloclient: Skipping send audio as peer is not connected\n");
        return;
    }

    ENetPacket *packet;
    allo_mjpeg_encode(pixels, pixels_wide, pixels_high, create_packet, &packet);
    
    const int headerlen = sizeof(int32_t); // track id header
    int32_t big_track_id = htonl(track_id);
    memcpy(packet->data, &big_track_id, headerlen);

    int ok = enet_peer_send(_internal(client)->peer, CHANNEL_MEDIA, packet);
    allo_statistics.bytes_sent[0] += packet->dataLength;
    allo_statistics.bytes_sent[1+CHANNEL_MEDIA] += packet->dataLength;
    assert(ok == 0);
}