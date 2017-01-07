//
// Created by jun on 17-1-7.
//

#ifndef STREAMDECODER_STREAM_DECODER_H
#define STREAMDECODER_STREAM_DECODER_H
#include <stdint.h>

typedef struct StreamDecoder_T StreamDecoder;

typedef struct {
    uint32_t size;
    uint8_t *data;
} ENCFrame;

typedef struct {
    int32_t width;
    int32_t height;
    uint32_t size;
    uint8_t *data;
} RGBFrame;

enum STREAM_TYPE {
    STREAM_TYPE_NONE,

    STREAM_TYPE_H264,
    STREAM_TYPE_HEVC,
#define STREAM_TYPE_H265 STREAM_TYPE_HEVC

    STREAM_TYPE_END,
};

enum PIXEL_FORMAT {
    PIXEL_FORMAT_NONE,

    PIXEL_FORMAT_BGR24,
    PIXEL_FORMAT_RGB24,

    PIXEL_FORMAT_END,
};

int stream_decoder_alloc(StreamDecoder **decoder, int stream_type, int pixel_fmt);

void stream_decoder_free(StreamDecoder *decoder);

/* maybe return a upside down picture, you should rotate yourself */
int stream_decoder_process_frame(StreamDecoder *decoder, ENCFrame* enc_frame, RGBFrame* dec_frame, int is_first_frame);

#endif //STREAMDECODER_STREAM_DECODER_H
