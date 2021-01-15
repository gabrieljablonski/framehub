#include "frame_queue.h"

#include <iostream>
#include <mutex>
#include <deque>
#include <condition_variable>

#include "frame.h"

namespace framehub {

FrameQueue::FrameQueue() {}

FrameQueue::FrameQueue(size_t max_frames) : max_frames_(max_frames) {}

void FrameQueue::Destroy() {
  for (auto it = frames_.cbegin(); it != frames_.cend(); ++it) {
    ((Frame*)*it)->Free(true);
  }
  frames_.clear();
}

size_t FrameQueue::Size() {
  return frames_.size();
}

void FrameQueue::PushBack(Frame *frame) {
  std::unique_lock<std::mutex> lk(mutex_);
  frames_.push_back(frame);
  while (frames_.size() > max_frames_) {
    frames_.front()->ReleaseAll();
    frames_.pop_front();
  }
  lk.unlock();
  cv_.notify_all();
}

Frame* FrameQueue::Front() {
  std::unique_lock<std::mutex> lk(mutex_);
  if (!frames_.size()) {
    cv_.wait(lk);
  }
  return frames_.front();
}

Frame* FrameQueue::PopFront() {
  std::unique_lock<std::mutex> lk(mutex_);
  if (!frames_.size()) {
    cv_.wait(lk);
  }

  Frame *frame = frames_.front();
  if (frame->GetConsumersWaiting() == 1)
    frames_.pop_front();

  return frame;
}

} // namespace framehub
