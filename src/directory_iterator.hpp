#pragma once

#include "glob_list.hpp"
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
  glob_list _ignores;
  std::mutex _mutex;
  std::vector<inode::ptr> _inodes;

  // Decend into subdirectories
  bool _recursive = true;

  // Inode compare function, default is by path
  compare_function _compare;

  // How the permissions found on disk are recorded in the inodes
  permission_mode _permissions = permission_mode::exact;

 public:
  sorted_directory_iterator() = default;

  explicit sorted_directory_iterator(
      const std::filesystem::path& path, const glob_list& ignores, bool recursive = true);

  explicit sorted_directory_iterator(
      const std::filesystem::path& path,
      const glob_list& ignores,
      permission_mode permissions,
      bool recursive = true);

  explicit sorted_directory_iterator(
      const std::filesystem::path& path,
      const glob_list& ignores,
      compare_function compare,
      bool recursive = true,
      permission_mode permissions = permission_mode::exact);

  // Reads the directory at rel below the root, with the paths of the inodes
  // relative to the root, so that the ignore patterns, which are relative to
  // the root, match as in a scan of the whole root.
  explicit sorted_directory_iterator(
      const std::filesystem::path& root,
      const std::filesystem::path& rel,
      const glob_list& ignores,
      permission_mode permissions,
      bool recursive);

  ~sorted_directory_iterator();

  // begin and end functions
  std::vector<inode::ptr>::iterator begin();
  std::vector<inode::ptr>::iterator end();

  // Root inode
  inode::ptr& root();
  const inode::ptr& root() const;

 private:
  void read_directory(
      const std::filesystem::path& abs, const std::filesystem::path& rel, inode::ptr& parent, const glob_list& ignores);
};

}  // namespace fstree
