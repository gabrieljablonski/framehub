#include "frame_queue.h"

#include <iostream>
#include <mutex>
#include <deque>
#include <condition_variable>

#include "frame.h"

namespace framehub {

FrameQueue::FrameQueue() {}

FrameQueue::~FrameQueue() {}

void FrameQueue::Destroy() {
  for (auto it = frames_.cbegin(); it != frames_.cend(); ++it) {
    ((Frame*)*it)->Free();
  }
  frames_.clear();
}

size_t FrameQueue::Size() {
  std::unique_lock<std::mutex> lk(mutex_);
  return frames_.size();
}

uint64_t FrameQueue::GetFrontNumber() {
  std::unique_lock<std::mutex> lk(mutex_);
  return frames_.size() ? frames_.front()->GetNumber() : 0;
}

void FrameQueue::PushBack(Frame *frame) {
  std::unique_lock<std::mutex> lk(mutex_);
  frames_.push_back(frame);
  lk.unlock();
  cv_.notify_all();
}

Frame* FrameQueue::PeekFront() {
  std::unique_lock<std::mutex> lk(mutex_);
  if (!frames_.size()) {
    cv_.wait(lk);
  }
  return frames_.front();
}

Frame* FrameQueue::CloneFront() {
  std::unique_lock<std::mutex> lk(mutex_);
  if (!frames_.size()) {
    cv_.wait(lk);
  }

  return frames_.front()->Clone();
}

void FrameQueue::TryPopFront() {
  std::unique_lock<std::mutex> lk(mutex_);
  if (!frames_.size() || !frames_.front()->ShouldDispose())
    return;

  frames_.front()->Free();
  frames_.pop_front();
}

} // namespace framehub
