#include "remote.hpp"

#include "remote_jolt.hpp"

namespace fstree {

// remote::create

std::unique_ptr<remote> remote::create(const url& address) {
  if (address.scheme() == "jolt" || address.scheme() == "tcp") {
    return std::make_unique<remote_jolt>(address);
  }
  return nullptr;
}

}  // namespace fstree
