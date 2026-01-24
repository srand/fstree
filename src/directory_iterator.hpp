#pragma once

#include "ignore.hpp"
#include "inode.hpp"

#include <filesystem>
#include <functional>
#include <mutex>
#include <vector>

namespace fstree {

class pool;

// Recursive directory iterator
// Reads the directory recursively and sorts the inodes by path
class sorted_directory_iterator {
 public:
  using compare_function = std::function<bool(const inode::ptr&, const inode::ptr&)>;

 private:
  inode::ptr _root;
  pool* _pool;
  ignore_list _ignores;
  std::mutex _mutex;
  std::vector<inode::ptr> _inodes;

  // Decend into subdirectories
  bool _recursive = true;

  // Inode compare function, default is by path
  compare_function _compare;

 public:
  sorted_directory_iterator() = default;

  explicit sorted_directory_iterator(
      const std::filesystem::path& path, const ignore_list& ignores, bool recursive = true);

  explicit sorted_directory_iterator(
      const std::filesystem::path& path, const ignore_list& ignores, compare_function compare, bool recursive = true);

  ~sorted_directory_iterator();

  // begin and end functions
  std::vector<inode::ptr>::iterator begin();
  std::vector<inode::ptr>::iterator end();

  // Root inode
  inode::ptr& root();
  const inode::ptr& root() const;

 private:
  void read_directory(
      const std::filesystem::path& abs, const std::filesystem::path& rel, inode::ptr& parent, const ignore_list& ignores);
};

}  // namespace fstree
