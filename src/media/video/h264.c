#include "h264.h"
#include "../../client/_client.h"
#include "../../util.h"
#include <string.h>
#include <assert.h>
#include <libswscale/swscale.h>

static void yuv2rgb(uint8_t yValue, uint8_t uValue, uint8_t vValue,
        uint8_t *r, uint8_t *g, uint8_t *b) {
    int rTmp = yValue + (1.370705 * (vValue-128));
    // or fast integer computing with a small approximation
    // rTmp = yValue + (351*(vValue-128))>>8;
    int gTmp = yValue - (0.698001 * (vValue-128)) - (0.337633 * (uValue-128));
    // gTmp = yValue - (179*(vValue-128) + 86*(uValue-128))>>8;
    int bTmp = yValue + (1.732446 * (uValue-128));
//    int bTmp = yValue + (443*(uValue-128))>>8;
    *r = MIN(MAX(rTmp, 0), 255);
    *g = MIN(MAX(gTmp, 0), 255);
    *b = MIN(MAX(bTmp, 0), 255);
}

static inline void yuv420p_to_rgb32(uint8_t* y, uint8_t* u, uint8_t* v, uint8_t *rgb, uint32_t width, uint32_t height) {
    for (uint32_t j = 0; j < height; j++) {
        for (uint32_t k = 0; k < width; k++) {
            yuv2rgb(*y, *u, *v, &rgb[0], &rgb[1], &rgb[2]);
            rgb[3] = 255;
            rgb += 4;
            y++;u++;v++;
        }
    }
}

static inline void rgb32_to_yuv420p(uint8_t *rgb, uint8_t* y, uint8_t* u, uint8_t* v, uint32_t width, uint32_t height) {
    for (uint32_t j = 0; j < height; j++) {
        for (uint32_t k = 0; k < width; k++) {
            uint8_t sR = (uint8_t)(rgb[0]);
            uint8_t sG = (uint8_t)(rgb[1]);
            uint8_t sB = (uint8_t)(rgb[2]);
            
            *y = (uint8_t)((66 * sR + 129 * sG + 25 * sB + 128) >> 8) + 16;
            
            if (0 == j % 2 && 0 == k % 2) {
                *u = (uint8_t)((-38 * sR - 74 * sG + 112 * sB + 128) >> 8) + 128;
                *v = (uint8_t)((112 * sR - 94 * sG - 18 * sB + 128) >> 8) + 128;
                
                u++;
                v++;
            }
            y++;
            rgb += 4; // 4 bytes
        }
    }
}


ENetPacket *allo_video_write_h264(allo_media_track *track, allopixel *pixels, int32_t pixels_wide, int32_t pixels_high)
{

    if (track->info.video.encoder.codec == NULL) {
        track->info.video.encoder.codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (track->info.video.encoder.codec == NULL) {
            fprintf(stderr, "No encoder\n");
            return NULL;
        }
        track->info.video.encoder.context = avcodec_alloc_context3(track->info.video.encoder.codec);
        track->info.video.picture = av_frame_alloc();
        
        track->info.video.encoder.context->width = track->info.video.width;
        track->info.video.encoder.context->height = track->info.video.height;
        track->info.video.encoder.context->time_base = av_d2q(1.0, 10);
        track->info.video.encoder.context->pix_fmt = AV_PIX_FMT_YUV420P;
        
        int ret = avcodec_open2(track->info.video.encoder.context, track->info.video.encoder.codec, NULL);
        if (ret != 0) {
            fprintf(stderr, "avcodec_open2 return %d when opening encoder\n", ret);
            return NULL;
        }
    }
    
    int ret = 0;
    
    AVFrame *frame = av_frame_alloc();
    frame->format = track->info.video.encoder.context->pix_fmt;
    frame->width = track->info.video.encoder.context->width;
    frame->height = track->info.video.encoder.context->height;
    frame->pts = ++track->info.video.framenr;
    ret = av_frame_get_buffer(frame, 0);
    assert(ret == 0);
    ret = av_frame_make_writable(frame);
    assert(ret == 0);
    
    // Convert from pixels RGBA into frame->data YUV
    struct SwsContext *sws_ctx = sws_getContext(
        pixels_wide,
        pixels_high,
        AV_PIX_FMT_RGBA,
        frame->width,
        frame->height,
        frame->format,
        SWS_BILINEAR, NULL, NULL, NULL
    );
    uint8_t *srcData[3] = { (uint8_t*)pixels, NULL, NULL };
    int srcStrides[3] = { pixels_wide * sizeof(allopixel), 0, 0 };
    int height = sws_scale(sws_ctx, srcData, srcStrides, 0, pixels_high, frame->data, frame->linesize);
    
    
    ret = avcodec_send_frame(track->info.video.encoder.context, frame);
    assert(ret == 0);
    
    AVPacket *avpacket = av_packet_alloc();
    // TODO: Might need to loop and receive multiple packets (because encoded frames can come out of order)
    ret = avcodec_receive_packet(track->info.video.encoder.context, avpacket);
    
    ENetPacket *packet = NULL;
    if (ret >= 0) {
        packet = enet_packet_create(NULL, avpacket->size + 4, 0);
        memcpy(packet->data + 4, avpacket->data, avpacket->size);
    } else {
        fprintf(stderr, "alloclient: Something went wrong in the h264 encoding: %d\n", ret);
    }
    
    av_packet_free(&avpacket);
    av_frame_free(&frame);
    
    return packet;
}

allopixel *allo_video_parse_h264(alloclient *client, allo_media_track *track, unsigned char *data, size_t length, int32_t *pixels_wide, int32_t *pixels_high)
{
    if (track->info.video.decoder.codec == NULL) {
        track->info.video.decoder.codec = avcodec_find_decoder(AV_CODEC_ID_H264);
        if (track->info.video.decoder.codec == NULL) {
            fprintf(stderr, "No decoder\n");
        }
        track->info.video.decoder.context = avcodec_alloc_context3(track->info.video.decoder.codec);
        track->info.video.picture = av_frame_alloc();
        
        track->info.video.decoder.context->width = track->info.video.width;
        track->info.video.decoder.context->height = track->info.video.height;
        int ret = avcodec_open2(track->info.video.decoder.context, track->info.video.decoder.codec, NULL);
        if (ret != 0) {
            fprintf(stderr, "avcodec_open2 return %d when opening decoder\n", ret);
        }
    }
    
    AVPacket avpacket;
    avpacket.size = length;
    avpacket.data = data;
    avpacket.pts = ++track->info.video.framenr;
    int ret = avcodec_send_packet(track->info.video.decoder.context, &avpacket);
    
    if (ret != 0) {
        fprintf(stderr, "avcodec_send_packet return %d\n", ret);
        return NULL;
    }
    ret = avcodec_receive_frame(track->info.video.decoder.context, track->info.video.picture);
    if (ret != 0) {
        fprintf(stderr, "avcodec_receive_frame return %d\n", ret);
        return NULL;
    }
    
    allopixel *pixels = (allopixel*)malloc(track->info.video.picture->width * track->info.video.picture->height * sizeof(allopixel));
    for (int h = 0; h < track->info.video.picture->height; h++) {
        for (int w = 0; w < track->info.video.picture->width; w++) {
            int i = h*track->info.video.picture->width + w;
            allopixel *p = &pixels[i];
            p->r = track->info.video.picture->data[0][i];
            p->g = p->r;//track->info.video.picture->data[1][i];
            p->b = p->r;//track->info.video.picture->data[2][i];
            p->a = 255;
        }
    }
    *pixels_wide = track->info.video.picture->width;
    *pixels_high = track->info.video.picture->height;
//                yuv420p_to_rgb32(
//                    track->info.video.picture->data[0],
//                    track->info.video.picture->data[1],
//                    track->info.video.picture->data[2],
//                    (uint8_t*)pixels,
//                    track->info.video.picture->width,
//                    track->info.video.picture->height
//                );
    return pixels;
}
