#pragma once
#include "ignore.hpp"
#include "inode.hpp"
#include "sha1.hpp"

#include <filesystem>
#include <string>
#include <vector>

#include <sys/stat.h>

namespace fstree {

class cache;
class remote;

class index {
  ignore_list _ignore;
  std::vector<inode*> _inodes;
  std::filesystem::path _root_path;
  inode _root;

 public:
  // Constructor
  index() = default;

  explicit index(const std::filesystem::path& root) : _root_path(root) {}

  explicit index(const std::filesystem::path& root, const ignore_list& ignore) : _ignore(ignore), _root_path(root) {}

  std::vector<inode*>::iterator begin() { return _inodes.begin(); }

  std::vector<inode*>::iterator end() { return _inodes.end(); }

  std::vector<inode*>::const_iterator begin() const { return _inodes.cbegin(); }

  std::vector<inode*>::const_iterator end() const { return _inodes.cend(); }

  std::vector<inode*>::size_type size() const { return _inodes.size(); }

  std::string root_path() const { return _root_path.string(); }

  inode& root() { return _root; }

  const inode& root() const { return _root; }

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
  void push_back(inode* inode) { _inodes.push_back(inode); }

  // Refreshes the index by scanning the filesystem
  void refresh();

  // Saves the index to a file
  void save(const std::filesystem::path& file) const;

  // Sorts the index by path
  void sort();

 private:
  void checkout_node(fstree::cache& c, inode* node, const std::filesystem::path& path);
  inode* find_node_by_path(const std::filesystem::path& path);
};

}  // namespace fstree
