// File: src/lock_file_posix.cpp

#include "lock_file.hpp"

#include <Windows.h>

namespace fstree {

// A lock file is a file that is created to indicate that a resource is in use.
lock_file::lock_file(const std::filesystem::path& path) : _path(path), _handle(INVALID_HANDLE_VALUE) {
  // Create parent directories
  std::filesystem::create_directories(path.parent_path());

  // Create lock file
  _handle = CreateFileA(
      _path.string().c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
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

  // Lock using LockFile()
  if (!LockFile(_handle, 0, 0, 0, 0)) {
    throw std::runtime_error("failed to lock file: " + _path.string());
  }

  _mutex.lock();

  return context(*this);
}

void lock_file::unlock() {
  _mutex.unlock();

  if (_handle == INVALID_HANDLE_VALUE) {
    throw std::runtime_error("lock file is invalid");
  }

  // Unlock using UnlockFile()
  if (!UnlockFile(_handle, 0, 0, 0, 0)) {
    throw std::runtime_error("failed to unlock file: " + _path.string());
  }
}

}  // namespace fstree
