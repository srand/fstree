#pragma once

#include "remote.hpp"
#include "url.hpp"

#include <mutex>
#include <vector>

namespace fstree {

class remote_http : public remote {
 public:
  remote_http(const url& remote_url);
  ~remote_http() override;

  // Returns true if the object with the given hash is present in the remote.
  virtual bool has_object(const fstree::digest& hash) override;

  // Finds trees and objects that are missing in the remote.
  virtual void has_tree(const std::vector<fstree::digest>& hashes, const missing_callback& callback) override;

  // Returns a vector of booleans indicating the presence of objects with the given hashes.
  virtual void has_objects(const std::vector<fstree::digest>& hashes, std::vector<bool>& presence) override;

  // Send the object with the given hash and path to the remote.
  virtual void write_object(const fstree::digest& hash, const std::filesystem::path& path) override;

  // Read the object with the given hash from the remote and write it to the given path.
  // The temp path is used to store the object temporarily before moving it to the final path.
  virtual void read_object(
      const fstree::digest& hash, const std::filesystem::path& path, const std::filesystem::path& temp) override;

  // Returns the URL of the object with the given hash. Objects are spread
  // over directories named by the first two and next four hex digits.
  std::string url_path(const fstree::digest& hash) const;

 private:
    bool head(const fstree::digest& hash);

 private:
  url _remote_url;

  // Easy handle on loan from the pool below.
  class handle;

  // Idle curl easy handles. Each keeps its connections to the remote open,
  // so reusing them saves setting up a connection, and a TLS session, for
  // every request.
  std::mutex _handles_mutex;
  std::vector<void*> _handles;
};

}  // namespace fstree
