#pragma once

#include "glob_list.hpp"
#include "inode.hpp"

#include <filesystem>
#include <string>
#include <vector>

#include <sys/stat.h>

namespace fstree {

class cache;
class remote;

class index {
  glob_list _ignore;
  std::vector<inode::ptr> _inodes;
  std::filesystem::path _root_path;
  inode::ptr _root;

  // How permissions are recorded by refresh() and restored by checkout()
  permission_mode _permissions = permission_mode::exact;

 public:
  // Constructor
  index();

  explicit index(const std::filesystem::path& root);

  explicit index(const std::filesystem::path& root, const glob_list& ignore);

  ~index();

  void dump() const;

  std::vector<inode::ptr>::iterator begin();
  std::vector<inode::ptr>::iterator end();
  std::vector<inode::ptr>::const_iterator begin() const;
  std::vector<inode::ptr>::const_iterator end() const;
  std::vector<inode::ptr>::size_type size() const;

  std::string root_path() const;

  inode::ptr& root();

  const inode::ptr& root() const;

  permission_mode permissions() const { return _permissions; }
  void set_permissions(permission_mode permissions) { _permissions = permissions; }

  // Checks out the index to the given path
  void checkout(fstree::cache& cache, const std::filesystem::path& path);

  // Refreshes the index with attributes from another index.
  // For example, an index of a remote repository tree can be updated
  // with inode attributes from a local index so that checkout of the
  // remote tree is faster.
  void copy_metadata(fstree::index& index);

  inode::ptr find_node_by_path(const std::filesystem::path& path) const;

  // Returns a list of inodes matching the given glob pattern
  std::vector<inode::ptr> glob(const std::string& pattern) const;
  std::vector<inode::ptr> glob(const glob_list& patterns) const;

  // Loads the index from the default .fstree/index file
  void load();

  // Loads the index from a file
  void load(const std::filesystem::path& file);

  // Load the ignore file from the index
  void load_ignore_from_index(fstree::cache& cache, const std::filesystem::path& path);

  // Adds patterns, each as a line of an ignore file, after those loaded
  void add_ignore_patterns(const std::vector<std::string>& patterns);

  // Looks up the hash of the inode at the given path in the index.
  bool lookup(const std::filesystem::path& path, fstree::digest& hash);

  void merge(const fstree::index& other);

  // Adds an inode to the index
  void push_back(inode::ptr inode);

  // Refreshes the index by scanning the filesystem
  void refresh();

  // Refreshes the index by scanning only the given paths, absolute or
  // relative to the root, and keeps the rest of the loaded index as it is.
  // Falls back to a full refresh, and returns false, if there are no paths
  // or the loaded index cannot be refreshed in part.
  bool refresh(const std::vector<std::filesystem::path>& paths);

  // Saves the index to the default .fstree/index file
  void save() const;

  // Saves the index to a file
  void save(const std::filesystem::path& file) const;

  // Sorts the index by path
  void sort();

 private:
  void checkout_node(fstree::cache& c, inode::ptr node, const std::filesystem::path& path);

  // Whether every node of the loaded index has a digest of the hash
  // algorithm in use and permissions recorded as the permission mode does,
  // so that the nodes outside the paths of a partial refresh can be kept.
  bool is_partially_refreshable() const;

  // Returns the path of an inode relative to the root, or an empty string
  // for the root itself.
  std::string relative_path(const std::filesystem::path& path) const;

  // Records the permissions of the inodes as the permission mode does, for a
  // tree that may have been written with another mode.
  void normalize_recorded_permissions();

  std::vector<inode::ptr> glob_linear(const std::string& patterns, std::vector<inode::ptr>& result) const;

  std::vector<inode::ptr> glob_recursive(const std::string& patterns, const inode::ptr& node, std::vector<inode::ptr>& result) const;

  void merge_recursive(inode::ptr& parent, const std::string& parent_path,
                       const std::vector<inode::ptr>& current_nodes,
                       const std::vector<inode::ptr>& other_nodes);

  // Helper methods for merging
  inode::ptr clone_node(const inode::ptr& source);

};

}  // namespace fstree
