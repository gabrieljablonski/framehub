#pragma once

#include <iostream>
#include <mutex>
#include <chrono>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

namespace framehub {

class Frame {
  private:
    AVFrame *frame_;
    int64_t dts_;
    int stream_;
    uint64_t number_;
    std::chrono::steady_clock::time_point live_until_;

  public:
    Frame();
    Frame(const Frame &f);
    Frame(AVFrame *frame, int64_t dts, int stream, uint64_t number);
    Frame(AVFrame *frame, int64_t dts, int stream, uint64_t number, std::chrono::steady_clock::time_point ttl_us);

    AVFrame* GetFrame();
    int64_t GetDts();
    int GetStream();
    uint64_t GetNumber();
    bool ShouldDispose();
    Frame* Clone();
    void Free();
};

}
