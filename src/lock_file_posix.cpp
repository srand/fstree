// File: src/lock_file_posix.cpp

#include "lock_file.hpp"

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

namespace fstree {

// Context class implementations
lock_file::context::context(lock_file& lock) : _lock(lock) {}
lock_file::context::~context() { _lock.unlock(); }

// A lock file is a file that is created to indicate that a resource is in use.
lock_file::lock_file(const std::filesystem::path& path) : _path(path), _fd(-1) {
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
    throw std::runtime_error("fock file is invalid");
  }

  // Lock using flock()
  if (flock(_fd, LOCK_EX) == -1) {
    throw std::runtime_error("failed to lock file: " + _path.string());
  }

  return context(*this);
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
