extern "C" {
#include "../../client/_client.h"
#include "../../util.h"
#include "mjpeg.h"
#include <string.h>
#include <assert.h>
}
#include <tinyjpeg/tiny_jpeg.h>
#include <richgel9999-jpegcompressor/jpgd.h>
#include <richgel9999-jpegcompressor/jpge.h>

static void create_packet(ENetPacket **packet, void* encoded, int size)
{
    if (*packet) {
        size_t acc = (*packet)->dataLength;
        enet_packet_resize(*packet, acc + size);
        memcpy((*packet)->data + acc, encoded, size);
    } else {
        *packet = enet_packet_create(NULL, size + 4, 0 /* unreliable */);
        memcpy((*packet)->data + 4, encoded, size);
    }
}

static ENetPacket* allo_mjpeg_encode(allo_media_track *track, allopicture *picture)
{
    ENetPacket *packet = NULL;
    assert(picture->format == allopicture_format_rgba8888 && "mjpeg encoder only supports RGBA picture data");
    if(picture->format != allopicture_format_rgba8888)
    {
        fprintf(stderr, "allonet/mjpeg: dropping frame: picture is format %d, only rgba supported\n", picture->format);
        goto end;
    }

    (void)track;
    tje_encode_with_func((tje_write_func*)create_packet, &packet, 1, picture->width, picture->height, 4, (const unsigned char*)picture->planes[0].rgba);

end:
    allopicture_free(picture);
    return packet;
}

static allopixel *allo_mjpeg_decode(uint8_t *jpegdata, size_t jpegdata_size, int32_t *pixels_wide, int32_t *pixels_high)
{
    int components;
    allopixel *pixels = (allopixel*)jpgd::decompress_jpeg_image_from_memory(jpegdata, jpegdata_size, (int*)pixels_wide, (int*)pixels_high, &components, 4, 0);
    return pixels;
}




static bool video_track_initialize(allo_media_track *track, cJSON *comp)
{
    if(track->type != allo_media_type_video) return false;
    cJSON *jformat = cJSON_GetObjectItemCaseSensitive(comp, "format");
    if (strcmp(cJSON_GetStringValue(jformat), "mjpeg") == 0) {
        track->info.video.format = allo_video_format_mjpeg;
        return true;
    }
    return false;
}

static void video_track_destroy(allo_media_track *track)
{
    (void)track;
}

static void parse_video(alloclient *client, allo_media_track *track, unsigned char *mediadata, size_t length, mtx_t *unlock_me)
{
    uint32_t track_id = track->track_id;
    int32_t wide = track->info.video.width, high = track->info.video.height;
    if (!client->video_callback) {
        mtx_unlock(unlock_me);
        return;
    }

    allopixel *pixels = allo_mjpeg_decode(mediadata, length, &wide, &high);
    
    mtx_unlock(unlock_me);
    
    if (pixels && client->video_callback(client, track_id, pixels, wide, high)) {
        free(pixels);
    }
}


extern "C" void allo_media_mjpeg_register(void)
{
    allo_media_subsystem *sys = (allo_media_subsystem*)malloc(sizeof(allo_media_subsystem));
    sys->parse = parse_video;
    sys->track_initialize = video_track_initialize;
    sys->track_destroy = video_track_destroy;
    sys->create_video_packet = allo_mjpeg_encode;
    allo_media_subsystem_register(sys);
    printf("allonet: initialized mjpeg media subsystem\n");
}
