//
// Created by jun on 17-1-6.
//
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include "stream_decoder.h"

//#define STREAM_DEBUG 1

#ifdef STREAM_DEBUG
#define LOGD(format, ...) fprintf(stderr, "Debug [%s:%d %s] : " format "\n", __FILE__, __LINE__, __FUNCTION__, ## __VA_ARGS__)
#else
#define LOGD(format, ...)
#endif

#define LOGI(format, ...) fprintf(stderr, "Info  [%s:%d %s] : " format "\n", __FILE__, __LINE__, __FUNCTION__, ## __VA_ARGS__)
#define LOGE(format, ...) fprintf(stderr, "ERROR [%s:%d %s] : " format "\n", __FILE__, __LINE__, __FUNCTION__, ## __VA_ARGS__)

typedef struct SwsContext SwsContext;
typedef enum AVPixelFormat AVPixelFormat;
typedef enum AVCodecID AVCodecID;

struct StreamDecoder_T {
    AVCodec         *codec;
    AVCodecContext  *codec_ctx;
    SwsContext      *sws_ctx;
    AVFrame         *yuv_frame;
    AVFrame         *rgb_frame;
    AVPixelFormat   output_format;
    uint32_t        rgb_frame_size;
    uint8_t         *rgb_frame_buf;
};

AVCodecID CODECID_TABLE[STREAM_TYPE_END] = { AV_CODEC_ID_NONE, AV_CODEC_ID_H264, AV_CODEC_ID_H265};
AVPixelFormat PIXELFORMAT_TABLE[PIXEL_FORMAT_END] = { AV_PIX_FMT_NONE, AV_PIX_FMT_BGR24, AV_PIX_FMT_RGB24};

//format: only support  AV_PIX_FMT_BGR24 and AV_PIX_FMT_RGB24
int stream_decoder_alloc(StreamDecoder **decoder, int stream_type, int pixel_fmt)
{
    int ret = 0;
    StreamDecoder *stream_decoder = NULL;

    if (stream_type >= STREAM_TYPE_END || stream_type <= STREAM_TYPE_NONE
        || pixel_fmt >= PIXEL_FORMAT_END || pixel_fmt <= PIXEL_FORMAT_NONE) {
        LOGE("stream type(%d) or pixel format(%d) is invalid",stream_type, pixel_fmt);
        goto ALLOC_FAIL;
    }

    stream_decoder = (StreamDecoder*)av_mallocz(sizeof(StreamDecoder));
    if (stream_decoder == NULL) {
        LOGE("alloc stream decoder fail!");
        goto ALLOC_FAIL;
    }

    /* register all the codecs */
    avcodec_register_all();

    /* find the h264 video decoder */
    stream_decoder->codec = avcodec_find_decoder(CODECID_TABLE[stream_type]);
    if (!stream_decoder->codec) {
        LOGE("codec not found!");
        goto ALLOC_FAIL;
    }
    /* alloc codec context */
    stream_decoder->codec_ctx = avcodec_alloc_context3(stream_decoder->codec);
    if (!stream_decoder->codec_ctx) {
        LOGE( "codec context alloc fail!");
        goto ALLOC_FAIL;
    }
    /* open the codec */
    if (avcodec_open2(stream_decoder->codec_ctx, stream_decoder->codec, NULL) < 0) {
        LOGE("could not open codec");
        goto ALLOC_FAIL;
    }
    // Allocate video frame
    stream_decoder->yuv_frame = av_frame_alloc();
    if(stream_decoder->yuv_frame == NULL) {
        LOGE("could not alloc yuv frame");
        goto ALLOC_FAIL;
    }

    // Allocate an AVFrame structure
    stream_decoder->rgb_frame = av_frame_alloc();
    if(stream_decoder->rgb_frame == NULL) {
        LOGE("could not alloc rgb frame");
        goto ALLOC_FAIL;
    }
    stream_decoder->output_format = PIXELFORMAT_TABLE[pixel_fmt];

    *decoder = stream_decoder;
    LOGI("stream decoder init success, type %d, pixel format %d!", stream_type, pixel_fmt);
    return 0;

ALLOC_FAIL:
    stream_decoder_free(stream_decoder);
    stream_decoder = NULL;
    return -1;

}

void stream_decoder_free(StreamDecoder *decoder)
{
    LOGI("stream decoder destroy!");
    if (decoder == NULL) {
        return;
    }
    if (decoder->sws_ctx) {
        sws_freeContext (decoder->sws_ctx);
        decoder->sws_ctx = NULL;
    }
    if (decoder->yuv_frame) {
        av_frame_free(&decoder->yuv_frame);
        decoder->yuv_frame = NULL;
    }
    if (decoder->rgb_frame) {
        av_frame_free(&decoder->rgb_frame);
        decoder->rgb_frame = NULL;
    }
    if (decoder->codec_ctx) {
        avcodec_free_context(&decoder->codec_ctx);
        decoder->codec_ctx = NULL;
    }
    if (decoder->rgb_frame_buf) {
        av_free(decoder->rgb_frame_buf);
        decoder->rgb_frame_buf = NULL;
    }
    av_free(decoder);
    decoder = NULL;
}

//maybe return a upside down picture, you should rotate yourself
int stream_decoder_process_frame(StreamDecoder *decoder, ENCFrame* enc_frame, RGBFrame* dec_frame, int is_first_frame)
{
    int got_picture = 0;
    int dec_picture_size = 0;
    AVPacket apt;

    av_init_packet(&apt);
    apt.data = enc_frame->data;
    apt.size = enc_frame->size;

    /*
     * avcodec_decode_video2 is deprecated, when new api is stable
     * should replace to new api avcodec_send_packet and avcodec_recive_frame
     */
    int av_result = avcodec_decode_video2(decoder->codec_ctx, decoder->yuv_frame, &got_picture, &apt);
    if (av_result < 0) {
        LOGE("decode failed: inputbuf = %p , input_framesize = %d", enc_frame->data, enc_frame->size);
        return -1;
    }

    /* obtain stream information from first frame */
    if (is_first_frame) {
        if (decoder->sws_ctx)
        {
            sws_freeContext(decoder->sws_ctx);
            decoder->sws_ctx = NULL;
        }

        LOGD("parse first frame, stream rect (%dx%d) format %d",
             decoder->codec_ctx->width, decoder->codec_ctx->height, decoder->codec_ctx->pix_fmt);

        decoder->sws_ctx = sws_getContext(decoder->codec_ctx->width, decoder->codec_ctx->height, decoder->codec_ctx->pix_fmt,
                                          decoder->codec_ctx->width, decoder->codec_ctx->height, decoder->output_format,
                                          SWS_BICUBIC, NULL, NULL, NULL);
        if (decoder->sws_ctx == NULL) {
            LOGE("init swscale context fail! stream rect(%dx%d) format %d",
                    decoder->codec_ctx->width, decoder->codec_ctx->height, decoder->codec_ctx->pix_fmt);
            return -1;
        }

        if (decoder->rgb_frame_buf) {
            av_free(decoder->rgb_frame_buf);
            decoder->rgb_frame_buf = NULL;
        }
        decoder->rgb_frame_size = av_image_get_buffer_size(decoder->output_format, decoder->codec_ctx->width, decoder->codec_ctx->height, 1);
        decoder->rgb_frame_buf = (uint8_t*)av_malloc(decoder->rgb_frame_size);
        if( decoder->rgb_frame_buf == NULL ) {
            LOGE("av malloc failed!");
            return -1;
        }

        int ret = av_image_fill_arrays(decoder->rgb_frame->data, decoder->rgb_frame->linesize ,decoder->rgb_frame_buf,
                             decoder->output_format, decoder->codec_ctx->width, decoder->codec_ctx->height, 1);
        if (ret < 0) {
            LOGE("av_image_fill_arrays fail!");
            return -1;
        }
    }

    if (got_picture) {
        sws_scale(decoder->sws_ctx, decoder->yuv_frame->data,
                  decoder->yuv_frame->linesize, 0, decoder->codec_ctx->height,
                  decoder->rgb_frame->data, decoder->rgb_frame->linesize);
        dec_frame->width = decoder->codec_ctx->width;
        dec_frame->height = decoder->codec_ctx->height;
        dec_frame->size = decoder->rgb_frame_size;
        dec_frame->data = decoder->rgb_frame->data[0];

        LOGD("result %u, %p ,%d" ,dec_frame->size, dec_frame->data, got_picture);
    }
    return got_picture;
}
