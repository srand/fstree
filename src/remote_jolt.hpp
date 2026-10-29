#pragma once

#include "jolt.grpc.pb.h"
#include "jolt.pb.h"
#include "remote.hpp"
#include "url.hpp"

#include <grpcpp/grpcpp.h>

namespace fstree {

class remote_jolt : public remote {
  std::shared_ptr<grpc::Channel> _channel;
  std::unique_ptr<fstree::CacheService::Stub> _client;
  // Token sent with each call over channels without TLS, which call
  // credentials aren't sent over. Over TLS, the channel sends the token.
  std::string _insecure_token;

  // Sends the token with a call over a channel without TLS.
  void authenticate(grpc::ClientContext& context) const;

 public:
  explicit remote_jolt(const url& address);

  void has_tree(const std::vector<fstree::digest>& hashes, const missing_callback& callback) override;
  bool has_object(const fstree::digest& hash) override;
  void has_objects(const std::vector<fstree::digest>& hashes, std::vector<bool>& presence) override;
  void write_object(const fstree::digest& hash, const std::filesystem::path& path) override;
  void read_object(
      const fstree::digest& hash, const std::filesystem::path& path, const std::filesystem::path& temp) override;
};

}  // namespace fstree
