#include "directory_iterator.hpp"
#include "thread_pool.hpp"
#include "wait_group.hpp"

#include <cstring>

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace fstree {

// Iterator methods
std::vector<inode::ptr>::iterator sorted_directory_iterator::begin() { 
  return _inodes.begin(); 
}

std::vector<inode::ptr>::iterator sorted_directory_iterator::end() { 
  return _inodes.end();
}

inode::ptr& sorted_directory_iterator::root() { 
  return _root; 
}

const inode::ptr& sorted_directory_iterator::root() const { 
  return _root; 
}

void sorted_directory_iterator::read_directory(
    const std::filesystem::path& abs, const std::filesystem::path& rel, inode::ptr& parent, const glob_list& ignores) {
  // Open the directory
  DIR* dir = opendir(abs.c_str());
  if (dir == nullptr) {
    throw std::runtime_error("Failed to open directory: " + abs.string() + ": " + std::strerror(errno));
  }

  fstree::wait_group wg;

  // Iterate the directory
  const struct dirent* entry;
  while ((entry = readdir(dir)) != nullptr) {
    // Get the path
    std::filesystem::path relpath = rel / entry->d_name;
    std::filesystem::path abspath = abs / entry->d_name;

    // Skip . and ..
    if (relpath.filename() == "." || relpath.filename() == ".." || relpath.filename() == ".fstree") {
      continue;
    }

    // Skip ignored directories
    if (entry->d_type == DT_DIR && ignores.match(relpath)) {
      continue;
    }

    // Skip anything that's not a directory, file or symlink.
    if (entry->d_type != DT_DIR && entry->d_type != DT_REG && entry->d_type != DT_LNK) {
      continue;
    }

    // Stat the file
    struct stat st;
    if (lstat((abs / entry->d_name).c_str(), &st) != 0) {
      continue;
    }

    // Read the target of the symlink
    std::filesystem::path target;
    if ((st.st_mode & S_IFMT) == S_IFLNK) {
      target = std::filesystem::read_symlink(abs / entry->d_name);
    }

    // convert mtime to uint64_t
#ifdef __APPLE__
    inode::time_type mtime = uint64_t(st.st_mtimespec.tv_sec) * 1000000000 + st.st_mtimespec.tv_nsec;
#else
    inode::time_type mtime = uint64_t(st.st_mtim.tv_sec) * 1000000000 + st.st_mtim.tv_nsec;
#endif

    // build status bits
    uint32_t status_bits = st.st_mode & ACCESSPERMS;
    switch ((st.st_mode & S_IFMT)) {
      case S_IFDIR:
        status_bits |= file_status::directory;
        break;
      case S_IFREG:
        status_bits |= file_status::regular;
        break;
      case S_IFLNK:
        status_bits |= file_status::symlink;
        break;
    }
    file_status status(status_bits);

    // Add the path to the list of inodes
    inode::ptr node = fstree::make_intrusive<inode>(relpath.string(), status, mtime, st.st_size, target);
    {
      std::lock_guard<std::mutex> lock(_mutex);
      _inodes.push_back(node);
      parent->add_child(node);
    }

    // Recurse if it's a directory
    if (_recursive && entry->d_type == DT_DIR) {
      wg.add(1);
      _pool->enqueue_or_run([this, abspath, relpath, node, &wg] {
        try {
          inode::ptr node_c = intrusive_ptr<inode>(node); 
          read_directory(abspath, relpath, node_c, _ignores);
          wg.done();
        }
        catch (const std::exception& e) {
          wg.exception(e);
        }
      });
    }
  }

  // Close the directory
  closedir(dir);

  // Wait for all the children to finish
  wg.wait_rethrow();
}

}  // namespace fstree
