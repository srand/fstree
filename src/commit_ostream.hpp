#pragma once

#include <functional>
#include <mutex>
#include <sstream>

namespace fstree {

// A thread-safe output stream that can be used to write to any std::ostream
// from multiple threads without interleaving the output.

class commit_ostream : public std::ostringstream {
  std::function<void(const std::string&)> _commit;

 public:
  commit_ostream() = default;

  virtual ~commit_ostream() {
    if (_commit) {
      _commit(str());
    }
  }

  // Construct a commit_ostream from a
  commit_ostream(std::function<void(const std::string&)> commit) : _commit(commit) {}

  // Move constructor
  commit_ostream(commit_ostream&& other) : std::ostringstream(std::move(other)), _commit(std::move(other._commit)) {}
};

}  // namespace fstree
