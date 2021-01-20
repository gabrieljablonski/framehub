#include "frame_queue.h"

#include <iostream>
#include <mutex>
#include <deque>
#include <condition_variable>

#include "frame.h"

namespace framehub {

FrameQueue::FrameQueue() : FrameQueue(0) {}

FrameQueue::FrameQueue(size_t min_size) : min_size_(min_size) {}

FrameQueue::~FrameQueue() {}

void FrameQueue::Destroy() {
  for (auto it = frames_.cbegin(); it != frames_.cend(); ++it) {
    ((Frame *)*it)->Free();
  }
  frames_.clear();
}

std::unique_lock<std::mutex> FrameQueue::GetLock() {
  std::unique_lock<std::mutex> lk(mutex_, std::defer_lock);
  return std::move(lk);
}

size_t FrameQueue::Size() {
  std::unique_lock<std::mutex> lk(mutex_);
  return frames_.size();
}

int64_t FrameQueue::GetNextNumber(uint8_t consumer_id) {
  std::unique_lock<std::mutex> lk(mutex_);
  for (auto it = frames_.cbegin(); it != frames_.cend(); ++it) {
    if (!((Frame *)*it)->WasConsumedBy(consumer_id))
      return ((Frame *)*it)->GetNumber();
  }
  return -1;
}

void FrameQueue::PushBack(Frame *frame) {
  std::unique_lock<std::mutex> lk(mutex_);
  frames_.push_back(frame);
}

Frame* FrameQueue::PeekFront() {
  std::unique_lock<std::mutex> lk(mutex_);
  if (!frames_.size())
    return NULL;
  return frames_.front();
}

Frame* FrameQueue::PeekNext(uint8_t consumer_id) {
  std::unique_lock<std::mutex> lk(mutex_);
  for (auto it = frames_.cbegin(); it != frames_.cend(); ++it) {
    if (!((Frame *)*it)->WasConsumedBy(consumer_id))
      return (Frame *)*it;
  }
  return NULL;
}

Frame* FrameQueue::CloneNext(uint8_t consumer_id) {
  std::unique_lock<std::mutex> lk(mutex_);
  for (auto it = frames_.begin(); it != frames_.end(); ++it) {
    if (!((Frame *)*it)->WasConsumedBy(consumer_id))
      return ((Frame *)*it)->Clone(consumer_id);
  }
  return NULL;
}

void FrameQueue::TryPopFront() {
  std::unique_lock<std::mutex> lk(mutex_);
  if (frames_.size() <= min_size_ || !frames_.front()->ShouldDispose())
    return;
  frames_.front()->Free();
  frames_.pop_front();
}

} // namespace framehub
