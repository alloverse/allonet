#include <allonet/client.h>

typedef void allo_mjpeg_encode_func(void *ctx, void* encoded, int size);

void allo_mjpeg_encode(allopixel *pixels, int32_t pixels_wide, int32_t pixels_high, allo_mjpeg_encode_func *encoder, void *ctx);
allopixel *allo_mjpeg_decode(uint8_t *jpegdata, size_t jpegdata_size, int32_t *pixels_wide, int32_t *pixels_high);
