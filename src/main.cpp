#include <string>

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
}

#include "helpers.h"

int main() {
    std::string filename("demo.mp4");
    AVFormatContext *pFormatContext = avformat_alloc_context();
    avformat_open_input(&pFormatContext, filename.c_str(), NULL, NULL);
    avformat_find_stream_info(pFormatContext, NULL);

    for(int i=0; i<pFormatContext->nb_streams; i++) {
        AVCodecParameters *pCP = pFormatContext->streams[i]->codecpar;
        const AVCodec *pCodec = avcodec_find_decoder(pCP->codec_id);

        if (pCP->codec_type == AVMEDIA_TYPE_VIDEO) {
            std::cout << "Video Codec: resolution " << pCP->width << "x" << pCP->height << " " << std::endl;
        } else if (pCP->codec_type == AVMEDIA_TYPE_AUDIO) {
            std::cout << "Audio Codec: " << pCP->channels << " channels, sample rate: " << pCP->sample_rate << std::endl;
        }
    }

    DumpAVFormat(pFormatContext);
}
