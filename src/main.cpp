#include <string>

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
}

#include "helpers.h"
#include "log.h"

int main() {
    int rc;
    std::string filename("demo.mp4");

    logging("init containers");
    AVFormatContext *pFormatContext = avformat_alloc_context();

    if (!pFormatContext) {
        logging("[ERROR] can not allocate memory for AVFormatContext");
        return -1;
    }

    rc = avformat_open_input(&pFormatContext, filename.c_str(), NULL, NULL);
    if (rc != 0) {
        logging("[ERROR] can not open input file: %s, return code: %d", filename.c_str(), rc);
        return -1;
    }

    rc = avformat_find_stream_info(pFormatContext, NULL);
    if (rc != 0) {
        logging("[ERROR] can not find stream info, return code: %d", rc);
        return -1;
    }

    logging("[INFO] Opened file %s.\nFormat: %s\nDuration: %lld\nBitRate: %lld\n",
            filename.c_str(), pFormatContext->iformat->long_name, pFormatContext->duration, pFormatContext->bit_rate);

    AVCodec *pCodec = NULL;
    AVCodecParameters *pCodecParameters = NULL;
    int video_stream_index = -1;

    for(int i=0; i<pFormatContext->nb_streams; i++) {
        AVCodecParameters *pLocalCP = pFormatContext->streams[i]->codecpar;

        AVCodec *pLocalCodec = avcodec_find_decoder(pLocalCP->codec_id);

        if (pLocalCodec == NULL) {
            logging("[ERROR] Unsupported codec: %s", pLocalCP->codec_id);
            continue;
        }

        if (pLocalCP->codec_type == AVMEDIA_TYPE_VIDEO) {
            if (video_stream_index == -1) {
                video_stream_index = i;
                pCodec = pLocalCodec;
                pCodecParameters = pLocalCP;
            }

            logging("[INFO] Video Codec. resolution: %d x %d", pLocalCP->width, pLocalCP->height);
        } else if (pLocalCP->codec_type == AVMEDIA_TYPE_AUDIO) {
            logging("[INFO] Audio Codec. %d channels, sample rate: %d", pLocalCP->channels, pLocalCP->sample_rate);
        }
        logging("\tCodec %s ID %d bit_rate %lld", pLocalCodec->name, pLocalCodec->id, pLocalCP->bit_rate);
    }

    if (video_stream_index == -1) {
        logging("[INFO] file %s does no contain video stream", filename.c_str());
        return 0;
    }

    AVCodecContext *pCodecContext = avcodec_alloc_context3(pCodec);
    if (!pCodecContext) {
        logging("[ERROR] failed to allocate memory for AVCodecContext");
        return -1;
    }

    rc = avcodec_parameters_to_context(pCodecContext, pCodecParameters);
    if (rc < 0) {
        logging("[ERROR] failed to copy codec params to codec context");
        return -1;
    }

    rc = avcodec_open2(pCodecContext, pCodec, NULL);
    if (rc < 0) {
        logging("[ERROR] failed to open codec");
        return -1;
    }

    AVFrame *pFrame = av_frame_alloc();
    if (!pFrame) {
        logging("[ERROR] failed to allocate memory for AVFrame");
        return -1;
    }

    AVPacket *pPacket = av_packet_alloc();
    if (!pPacket) {
        logging("[ERROR] failed to allocate memory for AVPacket");
        return -1;
    }

    int how_many_packets_to_process = 8;

    while (av_read_frame(pFormatContext, pPacket) >= 0) {
        if (pPacket->stream_index == video_stream_index) {
            logging("AVPacket->pts %d" PRId64, pPacket->pts);
            rc = Decode(pCodecContext, pPacket, pFrame);
            if (rc < 0) {
                logging("[ERROR] failed to decode");
                break;
            }
            if (--how_many_packets_to_process <= 0) break;
        }
        av_packet_unref(pPacket);

    }
    return 0;
}
