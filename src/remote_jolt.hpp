#pragma once

#include "jolt.grpc.pb.h"
#include "jolt.pb.h"
#include "remote.hpp"
#include "url.hpp"

#include <grpcpp/grpcpp.h>

#include <mutex>
#include <string>

namespace fstree {

class remote_jolt : public remote {
  std::shared_ptr<grpc::Channel> _channel;
  std::unique_ptr<fstree::CacheService::Stub> _client;
  // Token sent with each call over channels without TLS, which call
  // credentials aren't sent over. Over TLS, the channel sends the token.
  std::string _insecure_token;

  // Sends the token with a call over a channel without TLS.
  void authenticate(grpc::ClientContext& context) const;

  // Address of the server, for messages.
  std::string _address;
  // Set once the channel has connected.
  std::mutex _connect_mutex;
  bool _connected = false;

  // Waits for the channel to connect before the first call, and throws with
  // the reason if it doesn't. Calls wait for the server to be ready, which
  // would otherwise wait forever for a server that can't be connected to,
  // such as one with an untrusted certificate.
  void connect();

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
