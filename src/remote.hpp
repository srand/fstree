#pragma once

#include "digest.hpp"
#include "url.hpp"

#include <filesystem>
#include <memory>
#include <vector>

namespace fstree {

class url;

class remote {
 public:
  static std::unique_ptr<remote> create(const url& address);

  virtual ~remote() = default;

  // Returns true if the object with the given hash is present in the remote.
  virtual bool has_object(const fstree::digest& hash) = 0;

  // Returns lists of trees and objects that are missing in the remote.
  // The missing_trees and missing_objects vectors are filled with the hashes of the missing trees and objects.
  virtual void has_tree(
      const fstree::digest& hash, std::vector<fstree::digest>& missing_trees, std::vector<fstree::digest>& missing_objects) = 0;

  // Returns a vector of booleans indicating the presence of objects with the given hashes.
  virtual void has_objects(const std::vector<fstree::digest>& hashes, std::vector<bool>& presence) = 0;

  // Send the object with the given hash and path to the remote.
  virtual void write_object(const fstree::digest& hash, const std::filesystem::path& path) = 0;

  // Read the object with the given hash from the remote and write it to the given path.
  // The temp path is used to store the object temporarily before moving it to the final path.
  virtual void read_object(
      const fstree::digest& hash, const std::filesystem::path& path, const std::filesystem::path& temp) = 0;
};

}  // namespace fstree