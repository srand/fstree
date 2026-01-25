#include "remote.hpp"

#include "remote_jolt.hpp"

#ifdef FSTREE_ENABLE_HTTP_REMOTE
#include "remote_http.hpp"
#endif

#include <memory>

namespace fstree {

// remote::create

std::unique_ptr<remote> remote::create(const url& address) {
  if (address.scheme() == "jolt" || address.scheme() == "tcp") {
    return std::make_unique<remote_jolt>(address);
  }
#ifdef FSTREE_ENABLE_HTTP_REMOTE
  else if (address.scheme() == "http" || address.scheme() == "https") {
    return std::make_unique<remote_http>(address);
  }
#endif

  throw std::invalid_argument("unsupported remote scheme: " + address.scheme());
}

}  // namespace fstree
