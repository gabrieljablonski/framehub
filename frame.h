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
 public:
  Frame();
  Frame(const Frame &f);
  Frame(AVFrame *frame, int64_t dts, int stream, int64_t number);
  Frame(AVFrame *frame, int64_t dts, int stream, int64_t number, std::chrono::steady_clock::time_point ttl_us);

  AVFrame* GetFrame();
  int64_t GetDts();
  int GetStream();
  int64_t GetNumber();
  bool ShouldDispose();
  Frame* Clone(uint8_t consumer_id);
  bool WasConsumedBy(uint8_t consumer_id);
  void Free();

 private:
  AVFrame *frame_;
  int64_t dts_;
  int stream_;
  int64_t number_;
  uint64_t consumed_by_;
  std::chrono::steady_clock::time_point live_until_;
};

}
