#include <string>

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
}

#include "helpers.h"
#include "log.h"

int main() {
    int rc, nb_streams, stream_index, loop_times;
    int *streams_list = NULL;
    AVDictionary *opts = NULL;
    AVPacket packet;
    AVFormatContext *input_format_context = NULL;
    AVFormatContext *output_format_context = NULL;

    std::string input_filename("./demo.mp4");
    std::string output_filename("./output.ts");

    rc = avformat_open_input(&input_format_context, input_filename.c_str(), NULL, NULL);
    if (rc < 0) {
        logging("[ERROR] failed to open file %s", input_filename.c_str());
        goto end;
    }
    debug("opened input file %s. input_format_context: %d", input_filename.c_str(), &input_format_context);

    rc = avformat_find_stream_info(input_format_context, NULL);
    if (rc < 0) {
        logging("[ERROR] failed to find stream info");
        goto end;
    }
    debug("Format: %s\nDuration: %lld\nBitRate: %lld\n",
          input_format_context->iformat->long_name,
          input_format_context->duration,
          input_format_context->bit_rate);

    avformat_alloc_output_context2(&output_format_context, NULL, NULL, output_filename.c_str());
    if (!output_format_context) {
        logging("[ERROR] Could not create output context\n");
        rc = AVERROR_UNKNOWN;
        goto end;
    }
    debug("allocated output context: %d", &output_format_context);

    nb_streams = input_format_context->nb_streams;
    streams_list = static_cast<int *>(av_mallocz_array(nb_streams, sizeof(*streams_list)));

    if(!streams_list) {
        rc = AVERROR(ENOMEM);
        goto end;
    }
    debug("number of input streams: %d", nb_streams);

    stream_index = 0;
    for (int i=0; i<nb_streams; i++) {
        AVStream *out_stream;
        AVStream *in_stream = input_format_context->streams[i];

        AVCodecParameters *in_codec_params = in_stream->codecpar;

        debug("[%d] codec type %d", i, in_codec_params->codec_type);

        if (in_codec_params->codec_type != AVMEDIA_TYPE_AUDIO &&
                in_codec_params->codec_type != AVMEDIA_TYPE_VIDEO &&
                in_codec_params->codec_type != AVMEDIA_TYPE_SUBTITLE) {
            streams_list[i] = -1;
            continue;
        }

        streams_list[i] = stream_index++;
        out_stream = avformat_new_stream(output_format_context, NULL);
        if (!out_stream) {
            logging("[ERROR] failed to allocating output stream");
            rc = AVERROR_UNKNOWN;
            goto end;
        }
        debug("[%d] allocated output stream", i);

        rc = avcodec_parameters_copy(out_stream->codecpar, in_codec_params);
        if (rc < 0) {
            logging("[ERROR] failed to copy parameter from input stream");
            goto end;
        }
        debug("[%d] copied codec params", i);
    }

    av_dump_format(output_format_context, 0, output_filename.c_str(), 1);

    if(!(output_format_context->oformat->flags & AVFMT_NOFILE)) {
        rc = avio_open(&output_format_context->pb, output_filename.c_str(), AVIO_FLAG_WRITE);
        if (rc < 0) {
            logging("Could not open output file '%s'", output_filename.c_str());
            goto end;
        }
    }

    debug("open io file: %s", output_filename.c_str());

    rc = avformat_write_header(output_format_context, &opts);
    if (rc < 0) {
        logging("[ERROR] Error occurred when writing header");
        goto end;
    }
    debug("wrote headers: %s", output_filename.c_str());

    loop_times = 1;
    while (loop_times++) {
        AVStream *in_stream, *out_stream;
        rc = av_read_frame(input_format_context, &packet);
        if (rc < 0) break;
        in_stream = input_format_context->streams[packet.stream_index];

        if (packet.stream_index >= nb_streams || streams_list[packet.stream_index] < 0) {
            av_packet_unref(&packet);
            continue;
        }

        packet.stream_index = streams_list[packet.stream_index];
        out_stream = output_format_context->streams[packet.stream_index];

        packet.pts = av_rescale_q_rnd(packet.pts, in_stream->time_base, out_stream->time_base,
                                      static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        packet.dts = av_rescale_q_rnd(packet.dts, in_stream->time_base, out_stream->time_base,
                                      static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        packet.duration = av_rescale_q(packet.duration,  in_stream->time_base, out_stream->time_base);
        packet.pos = -1;

        debug("[%d] wrote packet, pts: %d; dts: %d; duration: %d", loop_times, packet.pts, packet.dts, packet.duration);
        rc = av_interleaved_write_frame(output_format_context, &packet);
        if (rc < 0) {
            logging("[ERROR] Error muxing packet");
            break;
        }

        av_packet_unref(&packet);
    }

    av_write_trailer(output_format_context);
    debug("wrote trailer: %s", output_filename.c_str());

end:
    avformat_close_input(&input_format_context);

    if (output_format_context && ! (output_format_context->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&output_format_context->pb);
    }

    avformat_free_context(output_format_context);
    av_freep(&streams_list);
    if (rc < 0 && rc != AVERROR_EOF) {
        logging("[ERROR] Error occurred: %s", av_err2string(rc).c_str());
        return -1;
    }

    return 0;
}
