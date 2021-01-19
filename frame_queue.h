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
  ~FrameQueue();
  std::unique_lock<std::mutex> GetLock();
  uint64_t GetFrontNumber();
  size_t Size();
  void Destroy();
  void PushBack(Frame *frame);
  Frame* PeekFront();
  Frame* CloneFront();
  void TryPopFront();
 
 private:
  std::deque<Frame *> frames_;
  std::mutex mutex_;
};

} // namespace framehub
