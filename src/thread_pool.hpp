#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

#include "semaphore.hpp"

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace fstree {

class pool {
 public:
  virtual ~pool() = default;

  // Enqueue a function to be executed by the thread pool
  virtual void enqueue(std::function<void()> f) = 0;

  // Enqueue a function to be executed by the thread pool, or run it immediately if all threads are busy
  virtual void enqueue_or_run(std::function<void()> f) = 0;
};

// A thread pool that can execute functions in parallel.
// The number of threads can be specified when creating the thread pool.
class thread_pool : public pool {
 public:
  explicit thread_pool(size_t num_threads);
  ~thread_pool();

  // Enqueue a function to be executed by the thread pool
  void enqueue(std::function<void()> f) override;

  // Enqueue a function to be executed by the thread pool, or run it immediately if all threads are busy
  void enqueue_or_run(std::function<void()> f) override;

  // Start all threads
  void start();

  // Wait for all threads to finish
  void stop();

 private:
  std::vector<std::thread> _threads;
  std::queue<std::function<void()>> _queue;
  std::mutex _mutex;
  std::condition_variable _cv;
  size_t _max_threads;
  bool _stop = false;
  semaphore _sem;
};

// A mock thread pool that executes functions directly
class direct_pool : public pool {
 public:
  explicit direct_pool(size_t num_threads);
  ~direct_pool();

  // Enqueue a function to be executed by the thread pool
  void enqueue(std::function<void()> f) override;
  void enqueue_or_run(std::function<void()> f) override;
};

pool& get_pool();

}  // namespace fstree

#endif  // THREAD_POOL_HPP
