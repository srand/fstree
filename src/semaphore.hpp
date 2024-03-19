#ifndef SEMAPHORE_HPP
#define SEMAPHORE_HPP

#include <condition_variable>
#include <mutex>

namespace fstree {

// A semaphore that can be used to synchronize threads
class semaphore {
 public:
  // Create a semaphore with the given initial count
  explicit semaphore(int count = 0) : _count(count) {}

  // Increment the count by one.
  // Notifies one waiting thread if there is one.
  void notify() {
    std::unique_lock<std::mutex> lock(_mutex);
    _count++;
    _cv.notify_one();
  }

  // Decrement the count by one.
  // This function blocks if the count reaches zero.
  void wait() {
    std::unique_lock<std::mutex> lock(_mutex);
    while (_count == 0) {
      _cv.wait(lock);
    }
    _count--;
  }

  // Try to decrement the count by one.
  // This function returns false if the count reaches zero.
  bool try_wait() {
    std::unique_lock<std::mutex> lock(_mutex);
    if (_count > 0) {
      _count--;
      return true;
    }
    return false;
  }

 private:
  std::mutex _mutex;
  std::condition_variable _cv;
  int _count;
};

}  // namespace fstree

#endif  // SEMAPHORE_HPP
