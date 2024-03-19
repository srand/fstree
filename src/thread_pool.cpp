#include "thread.hpp"
#include "thread_pool.hpp"

namespace fstree {

thread_pool::thread_pool(size_t max_threads) : _max_threads(max_threads), _stop(false), _sem(max_threads) { start(); }

thread_pool::~thread_pool() { stop(); }

void thread_pool::start() {
  for (size_t i = 0; i < _max_threads; i++) {
    _threads.emplace_back([this] {
      for (;;) {
        std::function<void()> task;
        {
          std::unique_lock<std::mutex> lock(_mutex);
          _cv.wait(lock, [this] { return _stop || !_queue.empty(); });
          if (_stop && _queue.empty()) {
            return;
          }
          task = std::move(_queue.front());
          _queue.pop();
        }
        task();
      }
    });
  }
}

void thread_pool::stop() {
  {
    std::unique_lock<std::mutex> lock(_mutex);
    _stop = true;
  }
  _cv.notify_all();
  for (size_t i = 0; i < _threads.size(); i++) {
    _threads[i].join();
  }
}

void thread_pool::enqueue(std::function<void()> f) {
  {
    std::unique_lock<std::mutex> lock(_mutex);
    _queue.push(f);
  }
  _cv.notify_one();
}

void thread_pool::enqueue_or_run(std::function<void()> f) {
  if (!_sem.try_wait()) {
    f();
  }
  else {
    enqueue([this, f] {
      f();
      _sem.notify();
    });
  }
}

direct_pool::direct_pool(size_t) {}

direct_pool::~direct_pool() {}

void direct_pool::enqueue(std::function<void()> f) { f(); }

void direct_pool::enqueue_or_run(std::function<void()> f) { f(); }

pool& get_pool() {
  static thread_pool pool(hardware_concurrency()); ;
  return pool;
}

}  // namespace fstree
