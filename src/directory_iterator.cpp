
#include "directory_iterator.hpp"
#include "thread_pool.hpp"

namespace fstree {

sorted_directory_iterator::sorted_directory_iterator(
    const std::filesystem::path& path, const ignore_list& ignores, bool recursive)
    : sorted_directory_iterator(path, ignores, [](const inode::ptr& a, const inode::ptr& b) { 
        return a->path() < b->path(); }, recursive)
{}
       
sorted_directory_iterator::sorted_directory_iterator(
    const std::filesystem::path& path, const ignore_list& ignores, compare_function compare, bool recursive) 
    : _root(fstree::make_intrusive<fstree::inode>())
    , _pool(&get_pool())
    , _ignores(ignores)
    , _recursive(recursive)
    , _compare(compare) 
{
    // Read the root directory and maybe recursively read the subdirectories
    read_directory(path, "", _root, ignores);

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
        std::remove_if(_inodes.begin(), _inodes.end(), [](const inode::ptr& inode) { return inode->is_ignored(); }),
        _inodes.end());
}

sorted_directory_iterator::~sorted_directory_iterator() {
    if (_root) {
        _root->clear();
    }
}

}  // namespace fstree
