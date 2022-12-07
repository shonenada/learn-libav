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
    DumpAVFormat(pFormatContext);
}
