#ifndef LEARN_LIBAV_HELPERS_H
#define LEARN_LIBAV_HELPERS_H

#include <iostream>

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
}

#include <log.h>

void DumpAVFormat(AVFormatContext *pCtx) {
    std::cout << "Format: " << pCtx->iformat->long_name
    << "\nDuration: " << pCtx->duration
    << "\nAudio Codec: " << pCtx->audio_codec_id
    << "\nVideo Codec: " << pCtx->video_codec_id
    << std::endl;
}

void save_grey_frame(unsigned char* buff, int wrap, int xsize, int ysize, char* filename) {
    FILE *f;
    f = fopen(filename, "w");

    // header of file pgm
    fprintf(f, "P5\n%d %d\n%d\n", xsize, ysize, 255);

    for (int i=0; i < ysize; i++) {
        fwrite(buff + i * wrap, 1, xsize, f);
    }

    fclose(f);
}

av_always_inline std::string av_err2string(int errnum) {
    char str[AV_ERROR_MAX_STRING_SIZE];
    return av_make_error_string(str, AV_ERROR_MAX_STRING_SIZE, errnum);
}

int Decode(AVCodecContext *pCodecContext, AVPacket *pPacket, AVFrame *pFrame) {
    int response = 0;
    response = avcodec_send_packet(pCodecContext, pPacket);
    if (response < 0) {
        logging("[ERROR] failed to sending packet to decoder: %s", av_err2string(response).c_str());
        return response;
    }
    while (response >= 0) {
        response = avcodec_receive_frame(pCodecContext, pFrame);
        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
            break;
        } else if (response < 0) {
            logging("[ERROR] failed to receive frame from decode: %s", av_err2string(response).c_str());
            return response;
        }

        if (response >= 0) {
            logging(
                "Frame %d (type=%c, size=%d bytes, format=%d) pts %d key_frame %d [DTS %d]",
                pCodecContext->frame_number,
                av_get_picture_type_char(pFrame->pict_type),
                pFrame->pkt_size,
                pFrame->format,
                pFrame->pts,
                pFrame->key_frame,
                pFrame->coded_picture_number
            );

            char frame_filename[1024];
            snprintf(frame_filename, sizeof(frame_filename), "%s-%d.pgm", "frame", pCodecContext->frame_number);
            if (pFrame->format != AV_PIX_FMT_YUV420P) {
                logging("Warning: format of frame is not AV_PIX_FMT_YUV420P");
            }
            save_grey_frame(pFrame->data[0], pFrame->linesize[0], pFrame->width, pFrame->height, frame_filename);
        }
    }
    return 0;
}

#endif //LEARN_LIBAV_HELPERS_H
