#pragma once

#include <iostream>
#include <mutex>

namespace fstree {

// A thread-safe output stream that can be used to write to any std::ostream
// from multiple threads without interleaving the output.

class mutex_ostream : public std::ostream {
  std::unique_lock<std::mutex> _lock;

 public:
  mutex_ostream() = default;

  // Construct a mutex_ostream from an existing std::ostream and a mutex
  mutex_ostream(std::ostream& stream, std::mutex& mutex) : std::ostream(stream.rdbuf()), _lock(mutex) {}

  // Move constructor
  mutex_ostream(mutex_ostream&& other) : std::ostream(other.rdbuf()), _lock(std::move(other._lock)) {}
};

}  // namespace fstree
