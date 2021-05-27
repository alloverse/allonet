#include "mjpeg.h"
#include <tinyjpeg/tiny_jpeg.h>
#include <richgel9999-jpegcompressor/jpgd.h>
#include <richgel9999-jpegcompressor/jpge.h>

extern "C" void allo_mjpeg_encode(allopixel *pixels, int32_t pixels_wide, int32_t pixels_high, allo_mjpeg_encode_func encoder, void *ctx)
{
    tje_encode_with_func((tje_write_func*)encoder, ctx, 1, pixels_wide, pixels_high, 4, (const unsigned char*)pixels);
}

extern "C" allopixel *allo_mjpeg_decode(uint8_t *jpegdata, size_t jpegdata_size, int32_t *pixels_wide, int32_t *pixels_high)
{
    int components;
    allopixel *pixels = (allopixel*)jpgd::decompress_jpeg_image_from_memory(jpegdata, jpegdata_size, (int*)pixels_wide, (int*)pixels_high, &components, 4, 0);
    return pixels;
}