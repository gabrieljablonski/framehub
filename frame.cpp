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
  : frame_(f.frame_), dts_(f.dts_), stream_(f.stream_), number_(f.number_), consumed_by_(f.consumed_by_) {}

Frame::Frame(AVFrame *frame, int64_t dts, int stream, int64_t number) 
  : Frame(frame, dts, stream, number, std::chrono::steady_clock::now()) {}

Frame::Frame(AVFrame *frame, int64_t dts, int stream, int64_t number, std::chrono::steady_clock::time_point live_until) 
  : frame_(frame), dts_(dts), stream_(stream), number_(number), live_until_(live_until), consumed_by_(0) {}

AVFrame* Frame::GetFrame() {
  return frame_;
}

int64_t Frame::GetDts() {
  return dts_;
}

int Frame::GetStream() {
  return stream_;
}

int64_t Frame::GetNumber() {
  return number_;
}

bool Frame::ShouldDispose() {
  return std::chrono::duration_cast<std::chrono::microseconds>(live_until_ - std::chrono::steady_clock::now()).count() < 0;
}

bool Frame::WasConsumedBy(uint8_t consumer_id) {
  return consumed_by_ & (1 << consumer_id);
}

Frame* Frame::Clone(uint8_t consumer_id) {
  consumed_by_ |= 1 << consumer_id;
  return new Frame(frame_ ? av_frame_clone(frame_) : NULL, dts_, stream_, number_);
}

void Frame::Free() {
  if (!frame_) return;
    av_frame_free(&frame_);
}

} // namespace framehub
