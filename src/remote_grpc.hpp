#pragma once

#include "cache.grpc.pb.h"
#include "cache.pb.h"
#include "remote.hpp"
#include "url.hpp"

#include <grpcpp/grpcpp.h>

namespace fstree {

class remote_grpc : public remote {
  std::shared_ptr<grpc::Channel> _channel;
  std::unique_ptr<fstree::CacheService::Stub> _client;

 public:
  explicit remote_grpc(const url& address);

  bool has_object(const std::string& hash) override;
  void has_objects(const std::vector<std::string>& hashes, std::vector<bool>& presence) override;

  void write_object(const std::string& hash, const std::filesystem::path& path) override;
  void read_object(
      const std::string& hash, const std::filesystem::path& path, const std::filesystem::path& temp) override;
};

}  // namespace fstree
