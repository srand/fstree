
#include "directory_iterator.hpp"
#include "thread_pool.hpp"

namespace fstree {

sorted_directory_iterator::sorted_directory_iterator(
    const std::filesystem::path& path, const glob_list& ignores, bool recursive)
    : sorted_directory_iterator(path, ignores, [](const inode::ptr& a, const inode::ptr& b) { 
        return a->path() < b->path(); }, recursive)
{}
       
sorted_directory_iterator::sorted_directory_iterator(
    const std::filesystem::path& path, const glob_list& ignores, compare_function compare, bool recursive) 
    : _root(fstree::make_intrusive<fstree::inode>())
    , _pool(&get_pool())
    , _ignores(ignores)
    , _recursive(recursive)
    , _compare(compare) 
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
