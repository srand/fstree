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
class sorted_recursive_directory_iterator {
  ignore_list _ignores;
  inode _root;
  pool& _pool;
  std::mutex _mutex;
  std::vector<inode*> _inodes;

 public:
  sorted_recursive_directory_iterator() = default;

#include <algorithm>

  explicit sorted_recursive_directory_iterator(const std::filesystem::path& path, const ignore_list& ignores)
      : _ignores(ignores), _pool(get_pool()) {
    // Read the directory recursively
    read_directory(path, "", &_root, ignores);

    // Sort the inodes by path
    std::sort(_inodes.begin(), _inodes.end(), [](const auto& a, const auto& b) { return a->path() < b->path(); });

    // Filter out ignored files and directories from the list in reverse order.
    for (auto it = _inodes.rbegin(); it != _inodes.rend();) {
      // Checking directories already in the read_directory function
      if ((*it)->is_directory()) {
        ++it;
        continue;
      }

      if (!(*it)->is_unignored() && _ignores.match((*it)->path())) {
        (*it)->ignore();
        it = decltype(it)(_inodes.erase(std::next(it).base()));
      }
      else {
        (*it)->unignore();
        ++it;
      }
    }
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
