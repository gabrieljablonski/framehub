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
  consumers_waiting_ = f.consumers_waiting_;
  stream_ = f.stream_;
  number_ = f.number_;
};

Frame::Frame(AVFrame *frame, int64_t dts, uint8_t consumers_waiting, int stream, uint64_t number) {
  frame_ = frame;
  dts_ = dts;
  consumers_waiting_ = consumers_waiting;
  stream_ = stream;
  number_ = number;
}

AVFrame* Frame::GetFrame() {
  return frame_;
}

int64_t Frame::GetDts() {
  return dts_;
}

uint8_t Frame::GetConsumersWaiting() {
  return consumers_waiting_;
}

int Frame::GetStream() {
  return stream_;
}

uint64_t Frame::GetNumber() {
  return number_;
}

Frame* Frame::Clone() {
  std::lock_guard<std::mutex> lk(mutex_);
  Frame *clone = new Frame(av_frame_clone(frame_), dts_, consumers_waiting_, stream_, number_);
  Release();
  return clone;
}

void Frame::Free(bool force) {
  if (!frame_) return;
  if (!consumers_waiting_ || force)
    av_frame_free(&frame_);
}

uint8_t Frame::Release() {
  if (consumers_waiting_)
    --consumers_waiting_;
  if (!consumers_waiting_)
    Free(false);
  return consumers_waiting_;
}

void Frame::ReleaseAll() {
  if (!consumers_waiting_) 
    Free(false);
  consumers_waiting_ = 0;
}

bool Frame::operator==(const Frame &other) {
  return frame_ == other.frame_;
}

} // namespace framehub
