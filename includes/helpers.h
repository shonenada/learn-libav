#ifndef LEARN_LIBAV_HELPERS_H
#define LEARN_LIBAV_HELPERS_H

#include <iostream>

extern "C" {
    #include <libavformat/avformat.h>
}

void DumpAVFormat(AVFormatContext *pCtx) {
    std::cout << "Format: " << pCtx->iformat->long_name
    << "\nDuration: " << pCtx->duration
    << "\nAudio Codec: " << pCtx->audio_codec_id
    << "\nVideo Codec: " << pCtx->video_codec_id
    << std::endl;
}

#endif //LEARN_LIBAV_HELPERS_H
