#pragma once

#include <iostream>
#include <mutex>
#include <deque>
#include <condition_variable>

#include "frame.h"

namespace framehub {

class FrameQueue {
 public:
  FrameQueue();
  FrameQueue(size_t min_size);
  ~FrameQueue();
  std::unique_lock<std::mutex> GetLock();
  int64_t GetNextNumber(uint8_t consumer_id);
  size_t Size();
  void Destroy();
  void PushBack(Frame *frame);
  Frame* PeekFront();
  Frame* PeekNext(uint8_t consumer_id);
  Frame* CloneNext(uint8_t consumer_id);
  void TryPopFront();
 
 private:
  size_t min_size_;
  std::deque<Frame *> frames_;
  std::mutex mutex_;
};

} // namespace framehub
