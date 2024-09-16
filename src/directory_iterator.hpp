#pragma once

#include "ignore.hpp"
#include "inode.hpp"
#include "thread.hpp"
#include "thread_pool.hpp"
#include "wait_group.hpp"

#include <filesystem>
#include <vector>

namespace fstree {

// Recursive directory iterator
// Reads the directory recursively and sorts the inodes by path
class sorted_directory_iterator {
 public:
  using compare_function = std::function<bool(inode*, inode*)>;

 private:
  inode _root;
  pool* _pool;
  ignore_list _ignores;
  std::mutex _mutex;
  std::vector<inode*> _inodes;

  // Decend into subdirectories
  bool _recursive = true;

  // Inode compare function, default is by path
  compare_function _compare;

 public:
  sorted_directory_iterator() = default;

  explicit sorted_directory_iterator(
      const std::filesystem::path& path, const ignore_list& ignores, bool recursive = true)
      : sorted_directory_iterator(path, ignores, [](inode* a, inode* b) { return a->path() < b->path(); }, recursive) {}

  explicit sorted_directory_iterator(
      const std::filesystem::path& path, const ignore_list& ignores, compare_function compare, bool recursive = true)
      : _pool(&get_pool()), _ignores(ignores), _recursive(recursive), _compare(compare) {
    // Read the root directory and maybe recursively read the subdirectories
    read_directory(path, "", &_root, ignores);

    // Sort the inodes by path
    std::sort(_inodes.begin(), _inodes.end(), _compare);

    // Filter out ignored files and directories from the list in reverse order.
    for (auto it = _inodes.rbegin(); it != _inodes.rend(); ++it) {
      // Checking directories already in the read_directory function
      if ((*it)->is_directory()) {
        continue;
      }

      if (!(*it)->is_unignored() && _ignores.match((*it)->path())) {
        (*it)->ignore();
      }
      else {
        (*it)->unignore();
      }
    }

    // Remove all ignored files and directories
    _inodes.erase(
        std::remove_if(_inodes.begin(), _inodes.end(), [](inode* inode) { return inode->is_ignored(); }),
        _inodes.end());
  }

  // begin and end functions
  std::vector<inode*>::iterator begin() { return _inodes.begin(); }
  std::vector<inode*>::iterator end() { return _inodes.end(); }

  // Root inode
  inode& root() { return _root; }

 private:
  void read_directory(
      const std::filesystem::path& abs, const std::filesystem::path& rel, inode* parent, const ignore_list& ignores);
};

}  // namespace fstree
