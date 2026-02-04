#pragma once

#include "digest.hpp"
#include "index.hpp"
#include "lock_file.hpp"
#include "remote.hpp"

#include <string>
#include <filesystem>

namespace fstree {

class cache {
  std::filesystem::path _objectdir, _tmpdir;
  size_t _max_size;
  size_t _max_size_slice;
  std::chrono::seconds _retention_period{3600};
  lock_file _lock;

 public:
  static std::filesystem::path default_path();
  static constexpr const char* default_max_size_string = "10GiB";
  static constexpr size_t default_max_size = 10ULL * 1024 * 1024 * 1024;  // 10 GiB
  static constexpr std::chrono::seconds default_retention = std::chrono::hours(1); 

 public:
  cache();

  // Constructor
  explicit cache(const std::filesystem::path& path, size_t max_size, std::chrono::seconds retention_period);

  // Retrieves the tree with the given hash from the cache.
  void read_tree(const fstree::digest& hash, inode::ptr& inode);

  // Creates a new index from the tree with the given hash.
  // The tree is traversed recursively.
  void index_from_tree(const fstree::digest& hash, fstree::index& index);
  
  // Returns true if the cache contains the tree object with the given hash.
  // The tree is not traversed recursively.
  bool has_tree(const fstree::digest& hash);

  // Returns true if the cache contains the object with the given hash.
  bool has_object(const fstree::digest& hash);

  // Fetch the object with the given hash from the remote and store it in the cache.
  void pull_object(fstree::remote& remote, const fstree::digest& hash);

  // Fetch the tree with the given hash from the remote and store it in the cache.
  void pull_tree(fstree::remote& remote, const fstree::digest& hash);

  // Push the object with the given hash to the remote.
  void push_object(fstree::remote& remote, const fstree::digest& hash);

  // Push the tree with the given hash to the remote.
  void push_tree(fstree::remote& remote, const fstree::digest& hash);

  // Adds an index to the cache.
  // Files/trees are copied/created into the cache object directory.
  void add(fstree::index& index);

  // Pull the tree with the given hash from the remote and add it to the index.
  // The tree is traversed recursively.
  void pull(fstree::index& index, fstree::remote& remote, const fstree::digest& tree);

  // Push the tree with the given hash to the remote.
  // The tree is traversed recursively.
  void push(const fstree::index& index, fstree::remote& remote);

  // Copy the object with the given hash to the given path.
  void copy_file(const fstree::digest& hash, const std::filesystem::path& to);

  // Evict objects from the cache until the size is below the maximum size.
  void evict();

  std::filesystem::path file_path(const inode::ptr& inode);
  std::filesystem::path tree_path(const inode::ptr& inode);

 private:
  void create_dirtree(inode::ptr& node);
  void create_file(const std::filesystem::path& root, const inode::ptr& inode);
  void evict_subdir(const std::filesystem::path& dir);

  std::filesystem::path file_path(const fstree::digest& hash);
  std::filesystem::path tree_path(const fstree::digest& hash);
};

}  // namespace fstree
