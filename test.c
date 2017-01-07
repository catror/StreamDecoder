//
// Created by jun on 17-1-7.
//

#include "stream_decoder.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define TEST_ALIGN(a, size) (((a) + ((size) - 1)) & ~((size) - 1))

static void dump_line(FILE *f, uint8_t* line, uint16_t n_pixel_bits, int width, int row_size)
{
    int i;
    int copy_bytes_size = TEST_ALIGN(n_pixel_bits * width, 8) / 8;

    fwrite(line, 1, copy_bytes_size, f);
    if (row_size > copy_bytes_size) {
        // each line should be 4 bytes aligned
        for (i = copy_bytes_size; i < row_size; i++) {
            fprintf(f, "%c", 0);
        }
    }
}

void dump_bitmap(RGBFrame *bitmap, int format)
{
    char file_str[200];
    uint16_t n_pixel_bits;
    uint32_t id;
    int row_size;
    uint32_t file_size;
    uint32_t header_size = 14 + 40;
    uint32_t bitmap_data_offset;
    uint32_t tmp_u32;
    int32_t tmp_32;
    uint16_t tmp_u16;
    FILE *f;

    switch (format) {
        case PIXEL_FORMAT_BGR24:
        case PIXEL_FORMAT_RGB24:
            n_pixel_bits = 24;
            break;
        default:
            fprintf(stderr, "invalid bitmap format  %u", format);
            return;
    }

    row_size = (((bitmap->width * n_pixel_bits) + 31) / 32) * 4;
    bitmap_data_offset = header_size;

    file_size = bitmap_data_offset + (bitmap->height * row_size);

    sprintf(file_str, "./image.bmp");

    f = fopen(file_str, "wb");
    if (!f) {
        fprintf(stderr, "Error creating bmp");
        return;
    }

    /* writing the bmp v3 header */
    fprintf(f, "BM");
    fwrite(&file_size, sizeof(file_size), 1, f);
    tmp_u16 = 0;
    fwrite(&tmp_u16, sizeof(tmp_u16), 1, f); // reserved for application
    tmp_u16 = 0;
    fwrite(&tmp_u16, sizeof(tmp_u16), 1, f);
    fwrite(&bitmap_data_offset, sizeof(bitmap_data_offset), 1, f);
    tmp_u32 = header_size - 14;
    fwrite(&tmp_u32, sizeof(tmp_u32), 1, f); // sub header size
    tmp_32 = bitmap->width;
    fwrite(&tmp_32, sizeof(tmp_32), 1, f);
    tmp_32 = -bitmap->height;
    fwrite(&tmp_32, sizeof(tmp_32), 1, f);

    tmp_u16 = 1;
    fwrite(&tmp_u16, sizeof(tmp_u16), 1, f); // color plane
    fwrite(&n_pixel_bits, sizeof(n_pixel_bits), 1, f); // pixel depth

    tmp_u32 = 0;
    fwrite(&tmp_u32, sizeof(tmp_u32), 1, f); // compression method

    tmp_u32 = 0; //file_size - bitmap_data_offset;
    fwrite(&tmp_u32, sizeof(tmp_u32), 1, f); // image size
    tmp_32 = 0;
    fwrite(&tmp_32, sizeof(tmp_32), 1, f);
    fwrite(&tmp_32, sizeof(tmp_32), 1, f);
    tmp_u32 = 0;
    fwrite(&tmp_u32, sizeof(tmp_u32), 1, f);
    tmp_u32 = 0;
    fwrite(&tmp_u32, sizeof(tmp_u32), 1, f);

    /* writing the data */
    for (int j = 0; j < bitmap->height; j++) {
        dump_line(f, bitmap->data + (j * bitmap->width * n_pixel_bits / 8), n_pixel_bits, bitmap->width, row_size);
    }

    fclose(f);
}


int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "you should specify a h264 stream file and give size!\n");
        return -1;
    }
    char filename[120] = {0};
    sprintf(filename, "./%s", argv[1]);
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        fprintf(stderr, "open %s fail\n", argv[1]);
    }
    char *end;
    int file_size = strtoul(argv[2], &end, 10);//62943;
    ENCFrame enc_frame;
    RGBFrame dec_frame;
    enc_frame.size = file_size;
    enc_frame.data = (uint8_t*)malloc(file_size);
    fread(enc_frame.data, file_size, 1, fp);
    StreamDecoder *decoder;
    if (stream_decoder_alloc(&decoder, STREAM_TYPE_H264, PIXEL_FORMAT_BGR24) < 0) {
        return -1;
    }
    if (stream_decoder_process_frame(decoder, &enc_frame, &dec_frame, 1) <=0) {
        fprintf(stderr, "decode fail\n");
    }
    dump_bitmap(&dec_frame, PIXEL_FORMAT_BGR24);
    stream_decoder_free(decoder);
    fclose(fp);
    fp = NULL;
}