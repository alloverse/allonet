#include <string.h>
#include <allonet/allonet.h>
#include "../media.h"
#include "../../util.h"

extern void _alloclient_send_audio(alloclient *client, int32_t track_id, const int16_t *pcm, size_t frameCount);
extern void allo_media_audio_register(void);