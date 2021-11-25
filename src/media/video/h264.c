#include "h264.h"
#include "../../client/_client.h"
#include "../../util.h"
#include <string.h>
#include <assert.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>


ENetPacket *allo_video_write_h264(allo_media_track *track, allopixel *pixels, int32_t pixels_wide, int32_t pixels_high)
{

    if (track->info.video.encoder.codec == NULL) {
        track->info.video.encoder.codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (track->info.video.encoder.codec == NULL) {
            fprintf(stderr, "No encoder\n");
            return NULL;
        }
        track->info.video.encoder.context = avcodec_alloc_context3(track->info.video.encoder.codec);
        
        track->info.video.encoder.context->width = track->info.video.width;
        track->info.video.encoder.context->height = track->info.video.height;
//        track->info.video.encoder.context->time_base = av_d2q(1.0, 10);
        track->info.video.encoder.context->time_base = (AVRational){9000, 1};
        track->info.video.encoder.context->pix_fmt = AV_PIX_FMT_YUV422P;
        track->info.video.encoder.context->bit_rate = 10 * 1000 * 1000;
        track->info.video.encoder.context->gop_size = 15;
        track->info.video.encoder.context->max_b_frames = 0;
//        track->info.video.encoder.context->rc_buffer_size = 0;
//        track->info.video.encoder.context->rc_max_rate = 0;
//        track->info.video.encoder.context->me_cmp = 1;
//        track->info.video.encoder.context->me_range = 0;
        track->info.video.encoder.context->thread_count = 1;
        track->info.video.encoder.context->codec_type = AVMEDIA_TYPE_VIDEO;
//        track->info.video.encoder.context->qmin = 10;
//        track->info.video.encoder.context->qmin = 50; // these affects quality a lot
        track->info.video.encoder.context->flags |= AV_CODEC_FLAG_LOOP_FILTER;
//        track->info.video.encoder.context->qcompress = 0.6;
//        track->info.video.encoder.context->i_quant_factor = 0.71; // no notable change
//        track->info.video.encoder.context->me_subpel_quality = 5; // no notable change
        track->info.video.encoder.context->refs = 3;
        track->info.video.encoder.context->trellis = 0;
        
        track->info.video.encoder.packet = av_packet_alloc();
        
        track->info.video.encoder.scale_context = NULL;
        
        av_opt_set(track->info.video.encoder.context->priv_data, "preset", "ultrafast", 0);
        av_opt_set(track->info.video.encoder.context->priv_data, "tune", "zerolatency", 0);
        av_opt_set(track->info.video.encoder.context->priv_data, "g", "30", 0);
                   
        int ret = avcodec_open2(track->info.video.encoder.context, track->info.video.encoder.codec, NULL);
        if (ret != 0) {
            fprintf(stderr, "avcodec_open2 return %d when opening encoder\n", ret);
            return NULL;
        }
    }
    
    int ret = 0;
    
    /// encoder may hold on to frame data pointer so no cheating
    AVFrame *frame = av_frame_alloc();
    frame->format = track->info.video.encoder.context->pix_fmt;
    frame->width = track->info.video.encoder.context->width;
    frame->height = track->info.video.encoder.context->height;
    frame->pts = track->info.video.framenr += 1;
    
    ret = av_frame_get_buffer(frame, 0);
    assert(ret == 0);
    
    // Convert from pixels RGBA into frame->data YUV
    struct SwsContext *sws_ctx = track->info.video.encoder.scale_context;
    sws_ctx = sws_getCachedContext(
        sws_ctx,
        pixels_wide,
        pixels_high,
        AV_PIX_FMT_RGBA,
        frame->width,
        frame->height,
        frame->format,
        SWS_FAST_BILINEAR, NULL, NULL, NULL
    );
    track->info.video.encoder.scale_context = sws_ctx;
    uint8_t *srcData[3] = { (uint8_t*)pixels, NULL, NULL };
    int srcStrides[3] = { pixels_wide * sizeof(allopixel), 0, 0 };
    int height = sws_scale(sws_ctx, srcData, srcStrides, 0, pixels_high, frame->data, frame->linesize);
    
    ret = avcodec_send_frame(track->info.video.encoder.context, frame);
    assert(ret == 0);
    
    AVPacket *avpacket = track->info.video.encoder.packet;
    // TODO: Might need to loop and receive multiple packets (because encoded frames can come out of order)
    ret = avcodec_receive_packet(track->info.video.encoder.context, avpacket);
    
    ENetPacket *packet = NULL;
    if (ret >= 0) {
        packet = enet_packet_create(NULL, avpacket->size + 4, 0);
        memcpy(packet->data + 4, avpacket->data, avpacket->size);
    } else {
        fprintf(stderr, "alloclient: Something went wrong in the h264 encoding: %d\n", ret);
    }
    av_packet_unref(avpacket);
    av_frame_unref(frame);
    
    
    av_frame_free(&frame);
    
    return packet;
}

allopixel *allo_video_parse_h264(alloclient *client, allo_media_track *track, unsigned char *data, size_t length, int32_t *pixels_wide, int32_t *pixels_high)
{
    assert(pixels_high != NULL);
    assert(pixels_wide != NULL);
    
    if (track->info.video.decoder.codec == NULL) {
        track->info.video.decoder.codec = avcodec_find_decoder(AV_CODEC_ID_H264);
        if (track->info.video.decoder.codec == NULL) {
            fprintf(stderr, "No decoder\n");
        }
        track->info.video.decoder.context = avcodec_alloc_context3(track->info.video.decoder.codec);
        track->info.video.decoder.frame = av_frame_alloc();
        
        track->info.video.decoder.packet = av_packet_alloc();
        
        track->info.video.decoder.context->width = track->info.video.width;
        track->info.video.decoder.context->height = track->info.video.height;
        
        track->info.video.decoder.scale_context = NULL;
        
        int ret = avcodec_open2(track->info.video.decoder.context, track->info.video.decoder.codec, NULL);
        if (ret != 0) {
            fprintf(stderr, "avcodec_open2 return %d when opening decoder\n", ret);
        }
    }
    
    AVPacket *avpacket = track->info.video.decoder.packet;
    avpacket->size = length;
    avpacket->data = data;
    int ret = avcodec_send_packet(track->info.video.decoder.context, avpacket);
    
    if (ret != 0) {
        fprintf(stderr, "avcodec_send_packet return %d\n", ret);
        return NULL;
    }
    
    AVFrame *frame = track->info.video.decoder.frame;
    ret = avcodec_receive_frame(track->info.video.decoder.context, frame);
    if (ret != 0) {
        fprintf(stderr, "avcodec_receive_frame return %d\n", ret);
        return NULL;
    }
    
    if (*pixels_wide == 0) *pixels_wide = frame->width;
    if (*pixels_high == 0) *pixels_high = frame->height;
    
    // Convert from frame->data YUV into pixels RGBA
    struct SwsContext *sws_ctx = track->info.video.decoder.scale_context;
    sws_ctx = sws_getCachedContext(
        sws_ctx,
        frame->width,
        frame->height,
        frame->format,
        *pixels_wide,
        *pixels_high,
        AV_PIX_FMT_RGBA,
        SWS_BICUBIC, NULL, NULL, NULL
    );
    track->info.video.decoder.scale_context = sws_ctx;
    
    // Convert pixel formats
    allopixel *pixels = (allopixel*)malloc((*pixels_wide) * (*pixels_high) * sizeof(allopixel));
    uint8_t *dstData[4] = { (uint8_t*)pixels, NULL, NULL, NULL };
    int dstStrides[4] = { (*pixels_wide) * sizeof(allopixel), 0, 0, 0 };
    int height = sws_scale(sws_ctx, frame->data, frame->linesize, 0, frame->height, dstData, dstStrides);

    av_frame_unref(frame);
    av_packet_unref(avpacket);
    return pixels;
}
