#pragma once

#include <iostream>
#include <mutex>

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
    uint8_t cloned_count_;
    std::mutex mutex_;

  public:
    Frame();
    Frame(const Frame &f);
    Frame(AVFrame *frame, int64_t dts, int stream, uint64_t number);

    AVFrame* GetFrame();
    int64_t GetDts();
    int GetStream();
    uint64_t GetNumber();
    uint8_t GetClonedCount();
    Frame* Clone();
    void Free();
    bool operator==(const Frame &other);
};

}
