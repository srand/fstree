
#include "directory_iterator.hpp"
#include "thread_pool.hpp"

#include <algorithm>

namespace fstree {

static bool by_path(const inode::ptr& a, const inode::ptr& b) { return a->path() < b->path(); }

sorted_directory_iterator::sorted_directory_iterator(
    const std::filesystem::path& path, const glob_list& ignores, bool recursive)
    : sorted_directory_iterator(path, ignores, by_path, recursive)
{}

sorted_directory_iterator::sorted_directory_iterator(
    const std::filesystem::path& path, const glob_list& ignores, permission_mode permissions, bool recursive)
    : sorted_directory_iterator(path, ignores, by_path, recursive, permissions)
{}

sorted_directory_iterator::sorted_directory_iterator(
    const std::filesystem::path& path,
    const glob_list& ignores,
    compare_function compare,
    bool recursive,
    permission_mode permissions)
    : _root(fstree::make_intrusive<fstree::inode>())
    , _pool(&get_pool())
    , _ignores(ignores)
    , _recursive(recursive)
    , _compare(compare)
    , _permissions(permissions)
{
    // Read the root directory and maybe recursively read the subdirectories
    read_directory(path, "", _root, ignores);

    // Sort the inodes by path. Ignored files and directories were already
    // left out by read_directory.
    std::sort(_inodes.begin(), _inodes.end(), _compare);
}

sorted_directory_iterator::~sorted_directory_iterator() {
    if (_root) {
        _root->clear();
    }
}

}  // namespace fstree
