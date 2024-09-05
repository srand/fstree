#pragma once

#include "filesystem.hpp"
#include "index.hpp"
#include "remote.hpp"

#include <string>

namespace fstree {

class cache {
  std::filesystem::path _objectdir, _tmpdir;
  size_t _max_size;
  size_t _max_size_slice;
  std::chrono::seconds _retention_period{3600};

 public:
  // Constructor
  explicit cache(const std::filesystem::path& path, size_t max_size, std::chrono::seconds retention_period);

  // Retrieves the tree with the given hash from the cache.
  void read_tree(const std::string& hash, inode& inode);

  // Creates a new index from the tree with the given hash.
  // The tree is traversed recursively.
  void index_from_tree(const std::string& hash, fstree::index& index);

  // Returns true if the cache contains the tree object with the given hash.
  // The tree is not traversed recursively.
  bool has_tree(const std::string& hash);

  // Returns true if the cache contains the object with the given hash.
  bool has_object(const std::string& hash);

  // Fetch the object with the given hash from the remote and store it in the cache.
  void pull_object(fstree::remote& remote, const std::string& hash);

  // Fetch the tree with the given hash from the remote and store it in the cache.
  void pull_tree(fstree::remote& remote, const std::string& hash);

  // Push the object with the given hash to the remote.
  void push_object(fstree::remote& remote, const std::string& hash);

  // Push the tree with the given hash to the remote.
  void push_tree(fstree::remote& remote, const std::string& hash);

  // Adds an index to the cache.
  // Files/trees are copied/created into the cache object directory.
  void add(fstree::index& index);

  // Pull the tree with the given hash from the remote and add it to the index.
  // The tree is traversed recursively.
  void pull(fstree::index& index, fstree::remote& remote, const std::string& tree);

  // Push the tree with the given hash to the remote.
  // The tree is traversed recursively.
  void push(const fstree::index& index, fstree::remote& remote);

  // Copy the object with the given hash to the given path.
  void copy(const std::string& hash, const std::filesystem::path& to);

  // Evict objects from the cache until the size is below the maximum size.
  void evict();

 private:
  void create_dirtree(inode* node);
  void create_file(const std::filesystem::path& root, const inode* inode);
  void evict_subdir(const std::filesystem::path& dir);

  std::filesystem::path file_path(const inode* inode);
  std::filesystem::path tree_path(const inode* inode);
  std::filesystem::path file_path(const std::string& hash);
  std::filesystem::path tree_path(const std::string& hash);
};

}  // namespace fstree
