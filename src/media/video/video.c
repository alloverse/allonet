#include "../../client/_client.h"
#include "../../util.h"
#include "../media.h"
#include <string.h>
#include <assert.h>

void _alloclient_send_video(alloclient *client, int32_t track_id, allopicture *picture)
{
    if (_internal(client)->peer == NULL) {
        fprintf(stderr, "alloclient: Skipping send video as we don't even have a peer\n");
        return;
    }
    
    if (_internal(client)->peer->state != ENET_PEER_STATE_CONNECTED) {
        fprintf(stderr, "alloclient: Skipping send video as peer is not connected\n");
        return;
    }
    
    alloclient_internal_shared *shared = _alloclient_internal_shared_begin(client);
    allo_media_track *track = _media_track_find(&shared->media_tracks, track_id);
    
    if (track == NULL) {
        fprintf(stderr, "alloclient: Skipping send video as track is not allocated\n");
        goto end;
    }
    
    if (track->type != allo_media_type_video) {
        fprintf(stderr, "alloclient: Skipping send video as track is not for video\n");
        goto end;
    }
    
    ENetPacket *packet = track->subsystem->create_video_packet(track, picture);
    
    if(packet) {
        const int headerlen = sizeof(int32_t); // track id header
        int32_t big_track_id = htonl(track_id);
        memcpy(packet->data, &big_track_id, headerlen);

        int ok = allo_enet_peer_send(_internal(client)->peer, CHANNEL_VIDEO, packet); (void)ok;
        bitrate_increment_sent(&track->bitrates, packet->dataLength);
        assert(ok == 0);
    }
end:
    _alloclient_internal_shared_end(client);
}
