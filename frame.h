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
    uint8_t consumers_waiting_;
    int stream_;
    std::mutex mutex_;
    uint64_t number_;

  public:
    Frame();
    Frame(const Frame &f);
    Frame(AVFrame *frame, int64_t dts, uint8_t consumers_waiting, int stream, uint64_t number);

    AVFrame* GetFrame();
    int64_t GetDts();
    uint8_t GetConsumersWaiting();
    int GetStream();
    uint64_t GetNumber();
    Frame* Clone();
    void Free(bool force);
    uint8_t Release();
    void ReleaseAll();
    bool operator==(const Frame &other);
};

}
