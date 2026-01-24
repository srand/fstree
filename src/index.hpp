#pragma once
#include "ignore.hpp"
#include "inode.hpp"

#include <filesystem>
#include <string>
#include <vector>

#include <sys/stat.h>

namespace fstree {

class cache;
class remote;

class index {
  ignore_list _ignore;
  std::vector<inode::ptr> _inodes;
  std::filesystem::path _root_path;
  inode::ptr _root;

 public:
  // Constructor
  index();

  explicit index(const std::filesystem::path& root);

  explicit index(const std::filesystem::path& root, const ignore_list& ignore);

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

  // Checks out the index to the given path
  void checkout(fstree::cache& cache, const std::filesystem::path& path);
  
  // Refreshes the index with attributes from another index.
  // For example, an index of a remote repository tree can be updated
  // with inode attributes from a local index so that checkout of the
  // remote tree is faster.
  void copy_metadata(fstree::index& index);

  // Loads the index from a file
  void load(const std::filesystem::path& file);

  // Load the ignore file from the index
  void load_ignore_from_index(fstree::cache& cache, const std::filesystem::path& path);

  // Adds an inode to the index
  void push_back(inode::ptr inode);

  // Refreshes the index by scanning the filesystem
  void refresh();

  // Saves the index to a file
  void save(const std::filesystem::path& file) const;

  // Sorts the index by path
  void sort();

 private:
  void checkout_node(fstree::cache& c, inode::ptr node, const std::filesystem::path& path);

  inode::ptr find_node_by_path(const std::filesystem::path& path);
  
  // Recursive refresh helper methods
  void refresh_recursive(const inode::ptr& tree_node, const inode::ptr& index_node);

};

}  // namespace fstree
