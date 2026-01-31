#include "remote.hpp"

#ifdef FSTREE_ENABLE_JOLT_REMOTE
#include "remote_jolt.hpp"
#endif

#ifdef FSTREE_ENABLE_HTTP_REMOTE
#include "remote_http.hpp"
#endif

#include <memory>

namespace fstree {

// remote::create

std::unique_ptr<remote> remote::create(const url& address) {
#ifdef FSTREE_ENABLE_JOLT_REMOTE
  if (address.scheme() == "jolt" || address.scheme() == "tcp") {
    return std::make_unique<remote_jolt>(address);
  }
#endif
#ifdef FSTREE_ENABLE_HTTP_REMOTE
  if (address.scheme() == "http" || address.scheme() == "https") {
    return std::make_unique<remote_http>(address);
  }
#endif

  throw std::invalid_argument("unsupported remote scheme: " + address.scheme());
}

}  // namespace fstree
