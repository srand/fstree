#ifdef _WIN32

#include "lock_file.hpp"

#include <Windows.h>

#include <map>

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
lock_file::lock_file(const std::filesystem::path& path)
    : _mutex(mutex_for_path(path)), _path(path), _handle(INVALID_HANDLE_VALUE) {
  // Create parent directories
  std::filesystem::create_directories(path.parent_path());

  // Create lock file
  _handle = CreateFileA(_path.string().c_str(), GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL, NULL);
  if (_handle == INVALID_HANDLE_VALUE) {
    throw std::runtime_error("failed to create lock file: " + _path.string());
  }
}

lock_file::~lock_file() {
  if (_handle != INVALID_HANDLE_VALUE) {
    CloseHandle(_handle);
  }
}

lock_file::context lock_file::lock() {
  if (_handle == INVALID_HANDLE_VALUE) {
    throw std::runtime_error("lock file is invalid");
  }

  std::unique_lock<std::mutex> mutex_lock(*_mutex);

  // Lock using LockFileEx().
  OVERLAPPED overlapped = {};
  if (!LockFileEx(_handle, LOCKFILE_EXCLUSIVE_LOCK, 0, 1, 0, &overlapped)) {
    throw std::runtime_error("failed to lock file: " + _path.string());
  }

  return context(*this, std::move(mutex_lock));
}

std::optional<lock_file::context> lock_file::try_lock() {
  if (_handle == INVALID_HANDLE_VALUE) {
    throw std::runtime_error("lock file is invalid");
  }

  std::unique_lock<std::mutex> mutex_lock(*_mutex, std::try_to_lock);
  if (!mutex_lock.owns_lock()) {
    return std::nullopt;
  }

  // Lock using LockFileEx() with LOCKFILE_FAIL_IMMEDIATELY.
  OVERLAPPED overlapped = {};
  if (!LockFileEx(_handle, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &overlapped)) {
    DWORD error = GetLastError();
    if (error == ERROR_LOCK_VIOLATION) {
      return std::nullopt;
    }
    throw std::runtime_error("failed to lock file: " + _path.string());
  }

  return context(*this, std::move(mutex_lock));
}

void lock_file::unlock() {
  if (_handle == INVALID_HANDLE_VALUE) {
    throw std::runtime_error("lock file is invalid");
  }

  // Unlock using UnlockFile()
  if (!UnlockFile(_handle, 0, 0, 1, 0)) {
    throw std::runtime_error("failed to unlock file: " + _path.string());
  }
}

}  // namespace fstree

#endif  // _WIN32
