#pragma once

#include <iostream>
#include <mutex>
#include <deque>
#include <condition_variable>

#include "frame.h"

namespace framehub {

class FrameQueue {
  private:
    std::deque<Frame *> frames_;
    std::mutex mutex_;

  public:
    FrameQueue();
    ~FrameQueue();
    void Lock();
    void Unlock();
    uint64_t GetFrontNumber();
    size_t Size();
    void Destroy();
    void PushBack(Frame *frame);
    Frame* PeekFront();
    Frame* CloneFront();
    void TryPopFront();
};

} // namespace framehub
