#include "remote.hpp"

#include "hash.hpp"

#ifdef FSTREE_ENABLE_JOLT_REMOTE
#include "remote_jolt.hpp"
#endif

#ifdef FSTREE_ENABLE_HTTP_REMOTE
#include "remote_http.hpp"
#endif

#include <memory>
#include <stdexcept>

namespace fstree {

void verify_download(const fstree::digest& hash, const std::filesystem::path& file) {
  if (hash.alg() != fstree::hash_function) {
    return;
  }

  fstree::digest actual = hashsum_hex_file(file);
  if (!(actual == hash)) {
    std::error_code ec;
    std::filesystem::remove(file, ec);
    throw std::runtime_error("downloaded object does not match its digest: " + hash.string() + ": got " + actual.string());
  }
}

// remote::create

std::unique_ptr<remote> remote::create(const url& address) {
#ifdef FSTREE_ENABLE_JOLT_REMOTE
  if (address.scheme() == "jolt" || address.scheme() == "tcp" || address.scheme() == "tls") {
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
