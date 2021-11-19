#include <string.h>
#include <allonet/allonet.h>
#include "../media.h"
#include "../../util.h"


void allo_audio_track_initialize(allo_media_track *track, cJSON *component);
void allo_audio_track_destroy(allo_media_track *track);
void allo_audio_parse(alloclient *client, allo_media_track *track, unsigned char *mediadata, size_t length, mtx_t *unlock_me);
void _alloclient_send_audio(alloclient *client, int32_t track_id, const int16_t *pcm, size_t frameCount);