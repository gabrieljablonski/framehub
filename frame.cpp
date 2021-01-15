#include "frame.h"

#include <iostream>
#include <mutex>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

namespace framehub {

Frame::Frame() {};

Frame::Frame(const Frame &f) {
  frame_ = f.frame_;
  dts_ = f.dts_;
  stream_ = f.stream_;
  number_ = f.number_;
  cloned_count_ = f.cloned_count_;
};

Frame::Frame(AVFrame *frame, int64_t dts, int stream, uint64_t number) {
  frame_ = frame;
  dts_ = dts;
  stream_ = stream;
  number_ = number;
  cloned_count_ = 0;
}

AVFrame* Frame::GetFrame() {
  return frame_;
}

int64_t Frame::GetDts() {
  return dts_;
}

uint8_t Frame::GetClonedCount() {
  return cloned_count_;
}

int Frame::GetStream() {
  return stream_;
}

uint64_t Frame::GetNumber() {
  return number_;
}

Frame* Frame::Clone() {
  std::lock_guard<std::mutex> lk(mutex_);
  Frame *clone = new Frame(av_frame_clone(frame_), dts_, stream_, number_);
  ++cloned_count_;
  return clone;
}

void Frame::Free() {
  if (!frame_) return;
    av_frame_free(&frame_);
}

bool Frame::operator==(const Frame &other) {
  return frame_ == other.frame_;
}

} // namespace framehub
