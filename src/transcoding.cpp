#include <string>

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/timestamp.h>
    #include <libavutil/opt.h>
}

#include "helpers.h"
#include "log.h"

typedef struct {
    char copy_video;
    char copy_audio;
    char *output_extension;
    char *muxer_opt_key;
    char *muxer_opt_value;
    char *video_codec;
    char *audio_codec;
    char *codec_priv_key;
    char *codec_priv_value;
} StreamingParams;

typedef struct {
    AVFormatContext *avfc;
    AVCodec *video_avc;
    AVCodec *audio_avc;
    AVStream *video_avs;
    AVStream *audio_avs;
    AVCodecContext *video_avcc;
    AVCodecContext *audio_avcc;
    int video_index;
    int audio_index;
    char *filename;
} StreamingContext;

int open_media(const char* in_filename, AVFormatContext **avfc) {
    debug("Calling open_media, filename: %s", in_filename);

    *avfc = avformat_alloc_context();
    if (!*avfc) {
        logging("[ERROR] failed to alloc memory for format");
        return -1;
    }

    int rc = avformat_open_input(avfc, in_filename, NULL, NULL);
    if (rc != 0) {
        logging("[ERROR] failed to open file %s", in_filename);
        logging("[ERROR] reason: %s", av_err2string(rc).c_str());
        return -1;
    }

    if (avformat_find_stream_info(*avfc, NULL) < 0) {
        logging("[ERROR] failed to get stream info");
        return -1;
    }

    return 0;
}

int fill_stream_info(AVStream *avs, AVCodec **avc, AVCodecContext **avcc) {
    *avc = avcodec_find_decoder(avs->codecpar->codec_id);

    if (!*avc) {
        logging("[ERROR] failed to find the codec");
        return -1;
    }

    *avcc = avcodec_alloc_context3(*avc);
    if (!*avcc) {
        logging("[ERROR] failed to alloc memory for codec context");
        return -1;
    }

    if (avcodec_parameters_to_context(*avcc, avs->codecpar) < 0) {
        logging("[ERROR] failed to fill codec context");
        return -1;
    }

    if (avcodec_open2(*avcc, *avc, NULL) < 0) {
        logging("[ERROR] failed to fill codec context");
        return -1;
    }

    if (avcodec_open2(*avcc, *avc, NULL) < 0)  {
        logging("failed to open codec");
        return -1;
    }

    return 0;
}

int prepare_decoder(StreamingContext *sc) {
    debug("calling prepare_decoder");
    debug("number of streams: %d", sc->avfc->nb_streams);
    for (int i=0; i< sc->avfc->nb_streams; i++) {
        auto codec_type = sc->avfc->streams[i]->codecpar->codec_type;
        if (codec_type == AVMEDIA_TYPE_VIDEO) {
            debug("[stream index %d] codec type: VIDEO", i);
            sc->video_avs = sc->avfc->streams[i];
            sc->video_index = i;
            if (fill_stream_info(sc->video_avs, &sc->video_avc, &sc->video_avcc)) {
                logging("[ERROR] failed to find video stream info");
                return -1;
            }
        } else if (codec_type == AVMEDIA_TYPE_AUDIO) {
            debug("[stream index %d] codec type: AUDIO", i);
            sc->audio_avs = sc->avfc->streams[i];
            sc->audio_index = i;
            if (fill_stream_info(sc->audio_avs, &sc->audio_avc, &sc->audio_avcc)) {
                logging("[ERROR] failed to find audio stream info");
                return -1;
            }
        } else {
            debug("[stream index %d] codec type: OTHER %d", i, codec_type);
            logging("[INFO] skipping stream other than audio and video");
        }
    }
    debug("finished call prepare_decoder");
    return 0;
}

int prepare_video_encoder(StreamingContext *sc, AVCodecContext *decoder_ctx, AVRational input_framerate, StreamingParams sp) {
    debug("calling prepare_video_encoder");
    sc->video_avs = avformat_new_stream(sc->avfc, NULL);

    debug("found video codec by name: %s", sp.video_codec);
    sc->video_avc = avcodec_find_encoder_by_name(sp.video_codec);
    if (!sc->video_avc) {
        logging("[ERROR] could not find the proper codec");
        return -1;
    }

    debug("allocate memory for video AVCodecContext");
    sc->video_avcc = avcodec_alloc_context3(sc->video_avc);
    if (!sc->video_avcc) {
        logging("[ERROR] could not allocated memory for codec context");
        return -1;
    }

    av_opt_set(sc->video_avcc->priv_data, "preset", "fast", 0);

    if (sp.codec_priv_key && sp.codec_priv_value) {
        av_opt_set(sc->video_avcc->priv_data, sp.codec_priv_key, sp.codec_priv_value, 0);
    }

    debug("decoder width: %d, height: %d, sar: %d", decoder_ctx->width, decoder_ctx->height, decoder_ctx->sample_aspect_ratio);
    sc->video_avcc->width = decoder_ctx->width;
    sc->video_avcc->height = decoder_ctx->height;
    sc->video_avcc->sample_aspect_ratio = decoder_ctx->sample_aspect_ratio;

    debug("pix_fmts: %d", sc->video_avc->pix_fmts);
    if (sc->video_avc->pix_fmts) {
        sc->video_avcc->pix_fmt = sc->video_avc->pix_fmts[0];
    } else {
        sc->video_avcc->pix_fmt = decoder_ctx->pix_fmt;
    }

    sc->video_avcc->bit_rate = 2 * 1000 * 1000;
    sc->video_avcc->rc_buffer_size = 4 * 1000 * 1000;
    sc->video_avcc->rc_max_rate = 2 * 1000 * 1000;
    sc->video_avcc->rc_min_rate = 2.5 * 1000 * 1000;

    sc->video_avcc->time_base = av_inv_q(input_framerate);
    sc->video_avs->time_base = sc->video_avcc->time_base;

    int rc = avcodec_open2(sc->video_avcc, sc->video_avc, NULL);
    if (rc < 0) {
        logging("[ERROR] could not open the codec: %s", av_err2string(rc).c_str());
        return -1;
    }

    rc = avcodec_parameters_from_context(sc->video_avs->codecpar, sc->video_avcc);
    if (rc < 0) {
        logging("[ERROR] could create params from context: %s", av_err2string(rc).c_str());
        return -1;
    }

    return 0;
}

int prepare_copy(AVFormatContext *avfc, AVStream **avs, AVCodecParameters *decoder_par) {
    debug("calling prepare copy");
    *avs = avformat_new_stream(avfc, NULL);
    debug("avformat_new_stream");
    avcodec_parameters_copy((*avs)->codecpar, decoder_par);
    debug("copy params");
    return 0;
}

int prepare_audio_encoder(StreamingContext *sc, int sample_rate, StreamingParams sp) {
    sc->audio_avs = avformat_new_stream(sc->avfc, NULL);
    sc->audio_avc = avcodec_find_encoder_by_name(sp.audio_codec);
    if (!sc->audio_avc) {
        logging("[ERROR] could not find the proper codec");
        return -1;
    }

    sc->audio_avcc = avcodec_alloc_context3(sc->audio_avc);
    if (!sc->audio_avcc) {
        logging("[ERROR] could not allocated memory for codec context");
        return -1;
    }

    int OUTPUT_CHANNELS = 2;
    int OUTPUT_BIT_RATE = 196000;
    sc->audio_avcc->channels = OUTPUT_CHANNELS;
    sc->audio_avcc->channel_layout = av_get_default_channel_layout(OUTPUT_CHANNELS);
    sc->audio_avcc->sample_rate = sample_rate;
    sc->audio_avcc->sample_fmt = sc->audio_avc->sample_fmts[0];
    sc->audio_avcc->bit_rate = OUTPUT_BIT_RATE;
    sc->audio_avcc->time_base = (AVRational) {1, sample_rate};
    sc->audio_avcc->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

    sc->audio_avs->time_base = sc->audio_avcc->time_base;

    if (avcodec_open2(sc->audio_avcc, sc->audio_avc, NULL) < 0) {
        logging("[ERROR] could not open the codec");
        return -1;
    }
    avcodec_parameters_from_context(sc->audio_avs->codecpar, sc->audio_avcc);
    return 0;
}

int remux(AVPacket **pkt, AVFormatContext **avfc, AVRational decoder_tb, AVRational encoder_tb) {
    av_packet_rescale_ts(*pkt, decoder_tb, encoder_tb);
    if (av_interleaved_write_frame(*avfc, *pkt) < 0) {
        logging("[ERROR] error while copying stream packet");
        return -1;
    }
    return 0;
}

int encode_video(StreamingContext *decoder, StreamingContext *encoder, AVFrame *input_frame) {
    if (input_frame) {
        input_frame->pict_type = AV_PICTURE_TYPE_NONE;
    }
    AVPacket *output_packet = av_packet_alloc();
    if (!output_packet) {
        logging("[ERROR] could not allocate memory for output AVPacket");
        return -1;
    }

    int rc = avcodec_send_frame(encoder->video_avcc, input_frame);

    while (rc >= 0) {
        rc = avcodec_receive_packet(encoder->video_avcc, output_packet);
        if (rc == AVERROR(EAGAIN) || rc == AVERROR_EOF) {
            break;
        } else if (rc < 0) {
            logging("[ERROR] Error while receiving packet from encoder: %s", av_err2string(rc).c_str());
            return -1;
        }

        output_packet->stream_index = decoder->video_index;
        output_packet->duration = encoder->video_avs->time_base.den / encoder->video_avs->time_base.num / decoder->video_avs->avg_frame_rate.num * decoder->video_avs->avg_frame_rate.den;

        av_packet_rescale_ts(output_packet, decoder->video_avs->time_base, encoder->video_avs->time_base);
        rc = av_interleaved_write_frame(encoder->avfc, output_packet);
        if (rc != 0) {
            logging("[ERROR] Error %d while receiving packet from decoder: %s", rc, av_err2string(rc).c_str());
            return -1;
        }

        av_packet_unref(output_packet);
        av_packet_free(&output_packet);
        return 0;
    }

    return 0;
}

int encode_audio(StreamingContext *decoder, StreamingContext *encoder, AVFrame *input_frame) {
    debug("call encode_audio");

    AVPacket *output_packet = av_packet_alloc();
    if (!output_packet) {
        logging("[ERROR] could not allocate memory for output AVPacket");
        return -1;
    }
    debug("allocate memory for output packet");

    int rc = avcodec_send_frame(encoder->audio_avcc, input_frame);
    if (rc < 0) {
        debug("nb_samples: %d; frame_size: %d", input_frame->nb_samples, encoder->audio_avcc->frame_size);
        logging("[ERROR] failed to send frame to encoder: %s", av_err2string(rc).c_str());
        return -1;
    }
    while (rc >= 0) {
        rc = avcodec_receive_packet(encoder->audio_avcc, output_packet);
        if (rc == AVERROR(EAGAIN) || rc == AVERROR_EOF) {
            break;
        } else if (rc < 0) {
            logging("[ERROR] Error while receiving packet from encoder: %s", av_err2string(rc).c_str());
            return -1;
        }

        output_packet->stream_index = decoder->audio_index;

        av_packet_rescale_ts(output_packet, decoder->audio_avs->time_base, encoder->audio_avs->time_base);
        rc = av_interleaved_write_frame(encoder->avfc, output_packet);
        if (rc != 0) {
            logging("[ERROR] Error %d while receiving packet from decoder: %s", rc, av_err2string(rc).c_str());
            return -1;
        }
    }

    av_packet_unref(output_packet);
    av_packet_free(&output_packet);

    return 0;
}

int transcode_video(StreamingContext *decoder, StreamingContext *encoder, AVPacket *input_packet, AVFrame *input_frame) {
    int rc = avcodec_send_packet(decoder->video_avcc, input_packet);
    if (rc < 0) {
        logging("[ERROR] Error while sending packet to decoder: %s", av_err2string(rc).c_str());
        return rc;
    }

    while (rc >= 0) {
        rc = avcodec_receive_frame(decoder->video_avcc, input_frame);
        if (rc == AVERROR(EAGAIN) || rc == AVERROR_EOF) {
            break;
        } else if (rc < 0) {
            logging("[ERROR] Error while receiving frame from decocder: %s", av_err2string(rc).c_str());
            return rc;
        }

        if (rc >= 0) {
            if (encode_video(decoder, encoder, input_frame)) {
                return -1;
            }
        }
        av_frame_unref(input_frame);
    }
    return 0;
}

int transcode_audio(StreamingContext *decoder, StreamingContext *encoder, AVPacket *input_packet, AVFrame *input_frame) {
    debug("transcode audio");

    int rc = avcodec_send_packet(decoder->audio_avcc, input_packet);
    if (rc < 0) {
        logging("[ERROR] Error while sending packet to decoder: %s", av_err2string(rc).c_str());
        return rc;
    }

    while (rc >= 0) {
        rc = avcodec_receive_frame(decoder->audio_avcc, input_frame);
        if (rc == AVERROR(EAGAIN) || rc == AVERROR_EOF) {
            debug("break as rc in (EAGAIN, AVERROR_EOF)");
            break;
        } else if (rc < 0) {
            logging("[ERROR] Error while receiving frame from decoder: %s", av_err2string(rc).c_str());
            return rc;
        }

        if (rc >= 0) {
            if (encode_audio(decoder, encoder, input_frame)) {
                logging("[ERROR] failed to encode audio");
                return -1;
            }
            av_frame_unref(input_frame);
        }
    }

    return 0;
}

int main() {
    /*
     * H264 -> H265
     * Audio -> remuxed (untouched)
     * MP4 - MP4
     */
//    StreamingParams sp = {0};
//    sp.copy_audio = 1;
//    sp.copy_video = 0;
//    sp.video_codec = "libx265";
//    sp.codec_priv_key = "x265-params";
//    sp.codec_priv_value = "keyint=60:min-keyint60:scenecut=0";
//    debug("H264 -> H265");

    /*
     * H264 -> H264 (fixed gop)
     * Audio -> remuxed (untouched)
     * MP4 - MP4
     */
    // StreamingParams sp = {0};
    // sp.copy_audio = 1;
    // sp.copy_video = 0;
    // sp.video_codec = "libx264";
    // sp.codec_priv_key = "x264-params";
    // sp.codec_priv_value = "keyint=60:min-keyint=60:scenecut=0:force-cfr=1";

    /*
     * H264 -> H264 (fixed gop)
     * Audio -> remuxed (untouched)
     * MP4 - fragmented MP4
     */
    // StreamingParams sp = {0};
    // sp.copy_audio = 1;
    // sp.copy_video = 0;
    // sp.video_codec = "libx264";
    // sp.codec_priv_key = "x264-params";
    // sp.codec_priv_value = "keyint=60:min-keyint=60:scenecut=0:force-cfr=1";
    // sp.muxer_opt_key = "movflags";
    // sp.muxer_opt_value = "frag_keyframe+empty_moov+delay_moov+default_base_moof";

    /*
     * H264 -> H264 (fixed gop)
     * Audio -> AAC
     * MP4 - MPEG-TS
     */
    // StreamingParams sp = {0};
    // sp.copy_audio = 0;
    // sp.copy_video = 0;
    // sp.video_codec = "libx264";
    // sp.codec_priv_key = "x264-params";
    // sp.codec_priv_value = "keyint=60:min-keyint=60:scenecut=0:force-cfr=1";
    // sp.audio_codec = "aac";
    // sp.output_extension = ".ts";

    /*
     * H264 -> VP9
     * Audio -> Vorbis
     * MP4 - WebM
     */
     StreamingParams sp = {0};
     sp.copy_audio = 0;
     sp.copy_video = 0;
     sp.video_codec = "libvpx-vp9";
     sp.audio_codec = "libvorbis";
     sp.output_extension = ".webm";

    StreamingContext *decoder = (StreamingContext*) calloc(1, sizeof(StreamingContext));
    char* decoder_filename = "demo.mp4";
    decoder->filename = decoder_filename;
    debug("Decoder filename: %s", decoder->filename);

    StreamingContext *encoder = (StreamingContext*) calloc(1, sizeof(StreamingContext));
    char encoder_filename[512] = "transcode.webm";
    encoder->filename = encoder_filename;
    debug("Encoder filename: %s", encoder->filename);

    if (sp.output_extension) {
        strcat(encoder->filename, sp.output_extension);
        debug("Encoder filename with extension: %s", sp.output_extension);
    }

    if (open_media(decoder->filename, &decoder->avfc)) {
        return -1;
    }

    if (prepare_decoder(decoder)) {
        return -1;
    }

    debug("alloc output context");
    avformat_alloc_output_context2(&encoder->avfc, NULL, NULL, encoder->filename);

    if (!encoder->avfc) {
        logging("[ERROR] could not allocate memory for output format");
        return -1;
    }

    debug("copy video if need: %d", sp.copy_video);
    if (!sp.copy_video) {
        AVRational input_framerate = av_guess_frame_rate(decoder->avfc, decoder->video_avs, NULL);
        debug("guess frame rate: num=%d, den=%d", input_framerate.num, input_framerate.den);

        prepare_video_encoder(encoder, decoder->video_avcc, input_framerate, sp);
    } else {
        prepare_copy(encoder->avfc, &encoder->video_avs, decoder->video_avs->codecpar);
    }

    debug("copy audio if need: %d", sp.copy_audio);
    debug(">>>> framerate %d; bit rate: %d", decoder->audio_avcc->framerate, decoder->audio_avcc->bit_rate);
    if (!sp.copy_audio) {
        if (prepare_audio_encoder(encoder, decoder->audio_avcc->sample_rate, sp)) {
            return -1;
        }
    } else {
        prepare_copy(encoder->avfc, &encoder->audio_avs, decoder->audio_avs->codecpar);
    }

    debug("encoder->avfc->oformat->flags & AVFMT_GLOBALHEADER = %d", encoder->avfc->oformat->flags & AVFMT_GLOBALHEADER);
    if (encoder->avfc->oformat->flags & AVFMT_GLOBALHEADER) {
        encoder->avfc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    debug("encoder->avfc->oformat->flags & AVFMT_NOFILE: %d", encoder->avfc->oformat->flags & AVFMT_NOFILE);
    if (!(encoder->avfc->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&encoder->avfc->pb, encoder->filename, AVIO_FLAG_WRITE) < 0) {
            logging("[ERROR] could not open the output file");
            return -1;
        }
    }

    AVDictionary *muxer_opts = NULL;

    debug("sp.muxer_opt_key && sp.muxer_opt_value: %d", sp.muxer_opt_key && sp.muxer_opt_value);
    if (sp.muxer_opt_key && sp.muxer_opt_value) {
        av_dict_set(&muxer_opts, sp.muxer_opt_key, sp.muxer_opt_value, 0);
    }

    debug("avformat_write_header");
    if (avformat_write_header(encoder->avfc, &muxer_opts) < 0) {
        logging("[ERROR] an error occurred when opening output file");
        return -1;
    }

    debug("allocate memory for input frame");
    AVFrame *input_frame = av_frame_alloc();
    if (!input_frame) {
        logging("[ERROR] failed to allocated memory for AVFrame");
        return -1;
    }

    debug("allocate memory for input packet");
    AVPacket *input_packet = av_packet_alloc();
    if (!input_packet) {
        logging("[ERROR] failed to allocated memory for AVPacket");
        return -1;
    }

    debug("loop for read frame");
    int times = 0;
    while(av_read_frame(decoder->avfc, input_packet) >= 0) {
        debug("times: %d", times);
        times += 1;
        if (decoder->avfc->streams[input_packet->stream_index]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            debug("handle video: %d", sp.copy_video);
            if (!sp.copy_video) {
                if (transcode_video(decoder, encoder, input_packet, input_frame)) {
                    return -1;
                }
                av_packet_unref(input_packet);
            } else {
                if (remux(&input_packet, &encoder->avfc, decoder->video_avs->time_base, encoder->video_avs->time_base)) {
                    return -1;
                }
            }
        } else if (decoder->avfc->streams[input_packet->stream_index]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            debug("handle audio: %d", sp.copy_audio);
            if (!sp.copy_audio) {
                if (transcode_audio(decoder, encoder, input_packet, input_frame)) {
                    return -1;
                }
                av_packet_unref(input_packet);
            } else {
                if (remux(&input_packet, &encoder->avfc, decoder->audio_avs->time_base, encoder->audio_avs->time_base)) {
                    return -1;
                }
            }
        } else {
            logging("[ERROR] ignoring all non video or audio packets");
        }
    }

    if (encode_video(decoder, encoder, NULL)) {
        return -1;
    }

    av_write_trailer(encoder->avfc);

    if (muxer_opts != NULL) {
        av_dict_free(&muxer_opts);
        muxer_opts = NULL;
    }

    if (input_packet != NULL) {
        av_packet_free(&input_packet);
        input_packet = NULL;
    }

    avformat_close_input(&decoder->avfc);
    avformat_free_context(decoder->avfc);
    decoder->avfc = NULL;
    avformat_free_context(encoder->avfc);
    encoder->avfc = NULL;

    avcodec_free_context(&decoder->video_avcc);
    decoder->video_avcc = NULL;
    avcodec_free_context(&encoder->video_avcc);
    encoder->video_avcc = NULL;

    free(decoder);
    decoder = NULL;
    free(encoder);
    encoder = NULL;

    return 0;
}
