#include <string>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

int main() {
    std::string filename("demo.mp4");
    AVFormatContext *pFormatContext = avformat_alloc_context();
    // avformat_open_input(&pFormatContext, filename.c_str(), NULL, NULL);
    avformat_open_input(&pFormatContext, "./demo.mp4", NULL, NULL);

}
