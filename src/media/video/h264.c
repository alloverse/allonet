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
        track->info.video.encoder.context->pix_fmt = AV_PIX_FMT_YUV422P;
        int ret = avcodec_open2(track->info.video.encoder.context, track->info.video.encoder.codec, NULL);
        if (ret != 0) {
            fprintf(stderr, "avcodec_open2 return %d when opening encoder\n", ret);
            return NULL;
        }
    }
    
    AVFrame *frame = av_frame_alloc();
    frame->format = AV_PIX_FMT_YUV422P;
    frame->width = track->info.video.width;
    frame->height = track->info.video.height;
    
    assert(av_frame_get_buffer(frame, 0) == 0);
    assert(av_frame_make_writable(frame) == 0);
    
    // Convert from pixels RGBA into frame->data YUV
    struct SwsContext *sws_ctx = sws_getContext(
                pixels_wide,
                pixels_high,
                AV_PIX_FMT_RGBA,
                frame->width,
                frame->height,
                AV_PIX_FMT_YUV420P,
                SWS_BILINEAR, NULL, NULL, NULL
                );
    int stride = pixels_wide * 4;
    int height = sws_scale(sws_ctx, (const uint8_t* const *)&pixels, &stride, 0, pixels_high, frame->data, frame->linesize);
    
    
    assert(avcodec_send_frame(track->info.video.encoder.context, frame) == 0);
    
    AVPacket avpacket;
    av_init_packet(&avpacket);
    int ret = 0;
    // TODO: Might need to loop and receive multiple packets (because encoded frames can come out of order)
    ret = avcodec_receive_packet(track->info.video.encoder.context, &avpacket);
    if (ret >= 0) {
        ENetPacket *packet = enet_packet_create(NULL, avpacket.size + 4, 0);
        memcpy(packet->data + 4, avpacket.data, avpacket.size);
        return packet;
    } else {
        fprintf(stderr, "alloclient: Something went wrong in the h264 encoding\n");
        return NULL;
    }
}

allopixel *allo_video_parse_h264(alloclient *client, allo_media_track *track, unsigned char *data, size_t length)
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