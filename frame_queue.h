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
    uint8_t consumer_count_;
    std::mutex mutex_;
    std::condition_variable cv_;

  public:
    FrameQueue();
    explicit FrameQueue(size_t max_frames);
    uint8_t GetConsumerCount();
    uint8_t AddConsumer();
    uint8_t RemoveConsumer();
    size_t Size();
    void Destroy();
    void PushBack(Frame *frame);
    Frame* Front();
    Frame* PopFront();
};

} // namespace framehub
