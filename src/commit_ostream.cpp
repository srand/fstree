#include "commit_ostream.hpp"

#include <functional>
#include <mutex>
#include <sstream>

namespace fstree {

// Constructor implementations
commit_ostream::commit_ostream() = default;

commit_ostream::~commit_ostream() {
  if (_commit) {
    _commit(str());
  }
}

commit_ostream::commit_ostream(std::function<void(const std::string&)> commit) : _commit(commit) {}

commit_ostream::commit_ostream(commit_ostream&& other) 
    : std::ostringstream(std::move(other)), _commit(std::move(other._commit)) {}

}  // namespace fstree