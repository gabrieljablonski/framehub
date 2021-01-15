#include "frame.h"

#include <iostream>
#include <mutex>
#include <chrono>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

namespace framehub {

Frame::Frame() {};

Frame::Frame(const Frame &f) 
  : frame_(f.frame_), dts_(f.dts_), stream_(f.stream_), number_(f.number_) {}

Frame::Frame(AVFrame *frame, int64_t dts, int stream, uint64_t number) 
  : Frame(frame, dts, stream, number, 0) {}

Frame::Frame(AVFrame *frame, int64_t dts, int stream, uint64_t number, uint64_t ttl_us) 
  : frame_(frame), dts_(dts), stream_(stream), number_(number) {
    std::chrono::microseconds ttl(ttl_us);
    live_until_ = std::chrono::steady_clock::time_point(std::chrono::steady_clock::now() + ttl);
}

AVFrame* Frame::GetFrame() {
  return frame_;
}

int64_t Frame::GetDts() {
  return dts_;
}

int Frame::GetStream() {
  return stream_;
}

uint64_t Frame::GetNumber() {
  return number_;
}

bool Frame::ShouldDispose() {
  return std::chrono::duration_cast<std::chrono::microseconds>(live_until_ - std::chrono::steady_clock::now()).count() < 0;
}

Frame* Frame::Clone() {
  return new Frame(frame_ ? av_frame_clone(frame_) : NULL, dts_, stream_, number_);
}

void Frame::Free() {
  if (!frame_) return;
    av_frame_free(&frame_);
}

} // namespace framehub
