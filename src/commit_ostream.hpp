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
  commit_ostream();

  virtual ~commit_ostream();

  // Construct a commit_ostream from a
  commit_ostream(std::function<void(const std::string&)> commit);

  // Move constructor
  commit_ostream(commit_ostream&& other);
};

}  // namespace fstree
