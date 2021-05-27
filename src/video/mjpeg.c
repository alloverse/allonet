#include "mjpeg.h"
#include <tinyjpeg/tiny_jpeg.h>
#include <nanojpeg/nanojpeg.h>

void allo_mjpeg_encode(allopixel *pixels, int32_t pixels_wide, int32_t pixels_high, allo_mjpeg_encode_func encoder, void *ctx)
{
    tje_encode_with_func((tje_write_func*)encoder, ctx, 1, pixels_wide, pixels_high, 4, (const unsigned char*)pixels);
}
