#include "thread_pool.hpp"

#include "thread.hpp"

namespace fstree {

thread_pool::thread_pool(size_t max_threads)
    : _max_threads(max_threads), _stop(false), _sem(max_threads), _jobserver(jobserver::create()) {}

thread_pool::~thread_pool() { stop(); }

// Workers are spawned lazily by enqueue(), so an idle machine or a low -j limit
// never pays for parked threads it will not use.
void thread_pool::spawn_worker() {
  _threads.emplace_back([this] {
    for (;;) {
      std::function<void()> task;
      {
        std::unique_lock<std::mutex> lock(_mutex);
        _idle++;
        _cv.wait(lock, [this] { return _stop || !_queue.empty(); });
        _idle--;
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
  std::unique_lock<std::mutex> lock(_mutex);
  _queue.push(f);
  if (_idle == 0 && _threads.size() < _max_threads) {
    spawn_worker();
  }
  else {
    _cv.notify_one();
  }
}

void thread_pool::enqueue_or_run(std::function<void()> f) {
  if (!_sem.try_wait()) {
    // No free worker slot; run inline.
    f();
    return;
  }

  char token = 0;
  bool have_token = false;
  if (_jobserver) {
    have_token = _jobserver->try_acquire(token);
    if (!have_token) {
      // The jobserver has no spare tokens; return the worker slot and run inline.
      _sem.notify();
      f();
      return;
    }
  }

  enqueue([this, f, token, have_token] {
    f();
    if (have_token) {
      _jobserver->release(token);
    }
    _sem.notify();
  });
}

direct_pool::direct_pool(size_t) {}

direct_pool::~direct_pool() {}

void direct_pool::enqueue(std::function<void()> f) { f(); }

void direct_pool::enqueue_or_run(std::function<void()> f) { f(); }

pool& get_pool() {
  static thread_pool pool(hardware_concurrency());
  ;
  return pool;
}

}  // namespace fstree
