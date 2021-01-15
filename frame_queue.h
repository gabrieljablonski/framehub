#pragma once

#include <iostream>
#include <mutex>
#include <deque>
#include <condition_variable>

#include "frame.h"

namespace framehub {

class FrameQueue {
  private:
    size_t max_frames_;
    std::deque<Frame *> frames_;
    std::mutex mutex_;
    std::condition_variable cv_;

  public:
    FrameQueue();
    explicit FrameQueue(size_t max_frames);
    size_t Size();
    void Destroy();
    void PushBack(Frame *frame);
    Frame* Front();
    Frame* PopFront();
};

} // namespace framehub
