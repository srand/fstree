
#ifndef _WIN32

#include "lock_file.hpp"

#include <cerrno>
#include <map>

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

namespace fstree {
namespace {
std::mutex lock_mutexes_mutex;
std::map<std::filesystem::path, std::weak_ptr<std::mutex>> lock_mutexes;

std::shared_ptr<std::mutex> mutex_for_path(const std::filesystem::path& path) {
  std::lock_guard<std::mutex> lock(lock_mutexes_mutex);
  auto& weak = lock_mutexes[path];
  auto mutex = weak.lock();
  if (!mutex) {
    mutex = std::make_shared<std::mutex>();
    weak = mutex;
  }
  return mutex;
}
}  // namespace



// Context class implementations
lock_file::context::context(lock_file& lock, std::unique_lock<std::mutex>&& mutex_lock)
    : _lock(&lock), _mutex_lock(std::move(mutex_lock)) {}
lock_file::context::~context() {
  if (_lock) {
    _lock->unlock();
  }
}

lock_file::context::context(context&& other) noexcept
    : _lock(other._lock), _mutex_lock(std::move(other._mutex_lock)) {
  other._lock = nullptr;
}

lock_file::context& lock_file::context::operator=(context&& other) {
  if (this != &other) {
    if (_lock) {
      _lock->unlock();
    }
    _lock = other._lock;
    _mutex_lock = std::move(other._mutex_lock);
    other._lock = nullptr;
  }
  return *this;
}

// A lock file is a file that is created to indicate that a resource is in use.
lock_file::lock_file(const std::filesystem::path& path) : _mutex(mutex_for_path(path)), _path(path), _fd(-1) {
  // Create parent directories
  std::filesystem::create_directories(_path.parent_path());

  // Create lock file
  _fd = open(_path.c_str(), O_CREAT | O_RDWR, 0666);
  if (_fd == -1) {
    throw std::runtime_error("failed to create lock file: " + _path.string());
  }
}

lock_file::~lock_file() {
  if (_fd != -1) {
    close(_fd);
  }
}

lock_file::context lock_file::lock() {
  if (_fd == -1) {
    throw std::runtime_error("lock file is invalid");
  }

  std::unique_lock<std::mutex> mutex_lock(*_mutex);

  // Lock using flock()
  if (flock(_fd, LOCK_EX) == -1) {
    throw std::runtime_error("failed to lock file: " + _path.string());
  }

  return context(*this, std::move(mutex_lock));
}

std::optional<lock_file::context> lock_file::try_lock() {
  if (_fd == -1) {
    throw std::runtime_error("lock file is invalid");
  }

  std::unique_lock<std::mutex> mutex_lock(*_mutex, std::try_to_lock);
  if (!mutex_lock.owns_lock()) {
    return std::nullopt;
  }

  // Lock using flock()
  if (flock(_fd, LOCK_EX | LOCK_NB) == -1) {
    int err = errno;
    if (err == EWOULDBLOCK || err == EAGAIN) {
      return std::nullopt;
    }
    throw std::runtime_error("failed to lock file: " + _path.string());
  }

  return context(*this, std::move(mutex_lock));
}

void lock_file::unlock() {
  if (_fd == -1) {
    throw std::runtime_error("lock file is invalid");
  }

  // Unlock using flock()
  if (flock(_fd, LOCK_UN) == -1) {
    throw std::runtime_error("failed to unlock file: " + _path.string());
  }
}

}  // namespace fstree

#endif  // _WIN32
