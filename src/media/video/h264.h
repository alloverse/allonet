#include <allonet/client.h>
#include "../media.h"

ENetPacket *allo_video_write_h264(allo_media_track *track, allopixel *pixels, int32_t pixels_wide, int32_t pixels_high);
allopixel *allo_video_parse_h264(alloclient *client, allo_media_track *track, unsigned char *mediadata, size_t length, int32_t *pixels_wide, int32_t *pixels_high);
