#pragma once

#include <condition_variable>
#include <mutex>

// A wait group is used to wait for a collection of parallel jobs to finish.
// It's similar to a semaphore, but it's used to wait for a fixed number of jobs to finish.
// The wait group starts with a count of zero, and each job increments the count.
// When a job finishes, it decrements the count by calling done().
// The wait() function blocks until the count reaches zero.

namespace fstree {

class wait_group {
 public:
  // Add delta to the count
  // If delta is negative, the count is decremented
  // If delta is zero, the count is not changed
  // The default value of delta is 1
  // This function is thread-safe
  void add(int delta = 1) {
    std::unique_lock<std::mutex> lock(_mutex);
    _count += delta;
  }

  // Decrement the count by one
  // This function is thread-safe
  // This function should be called by each job when it finishes
  // The wait() function will unblock when the count reaches zero
  void done() {
    std::unique_lock<std::mutex> lock(_mutex);
    _count--;
    _cv.notify_one();
  }

  // Wait for the count to reach zero
  // This function blocks until the count reaches zero
  // This function is thread-safe
  // This function should be called by the main thread
  void wait() {
    std::unique_lock<std::mutex> lock(_mutex);
    while (_count > 0) {
      _cv.wait(lock);
    }
  }

  // Wait for the count to reach zero and rethrow any exception that was caught by the wait group.
  void wait_rethrow() {
    wait();
    rethrow();
  }

  // // Report an error.
  // // This function should be called by a job when it encounters an error.
  // // The error code is stored and can be retrieved with the error() function.
  // // This function is thread-safe
  // void error(std::error_code ec) {
  //   std::unique_lock<std::mutex> lock(_mutex);
  //   _count--;
  //   _error = ec;
  //   _cv.notify_one();
  // }
  //
  // // Check if there was an error.
  // std::error_code error() const {
  //   std::unique_lock<std::mutex> lock(_mutex);
  //   return _error;
  // }

  // Add an exception to the wait group.
  // This function should be called by a job when it encounters an exception.
  // The exception is stored and can be retrieved with the exception() function.
  // Alternatively, the exception can be rethrown with rethrow().
  // This function is thread-safe
  void exception(const std::exception&) {
    std::unique_lock<std::mutex> lock(_mutex);
    _count--;
    _exception = std::current_exception();
    _cv.notify_one();
  }

  // Check if there was an exception.
  std::exception_ptr exception() const {
    std::unique_lock<std::mutex> lock(_mutex);
    return _exception;
  }

  // Rethrow any exception that was caught by the wait group.
  // This function should be called by the main thread after wait() returns.
  // This function is thread-safe
  void rethrow() {
    std::unique_lock<std::mutex> lock(_mutex);
    if (_exception) {
      std::rethrow_exception(_exception);
    }
  }

 private:
  // Mutex to protect the count
  mutable std::mutex _mutex;

  // Condition variable to wait for the count to reach zero
  std::condition_variable _cv;

  std::error_code _error;

  std::exception_ptr _exception;

  // The count of jobs
  int _count = 0;
};

}  // namespace fstree
