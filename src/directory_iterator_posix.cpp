#ifndef _WIN32

#include "directory_iterator.hpp"
#include "thread_pool.hpp"
#include "wait_group.hpp"

#include <cstring>
#include <memory>

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
  std::unique_ptr<DIR, int (*)(DIR*)> dir(opendir(abs.c_str()), closedir);
  if (!dir) {
    throw std::runtime_error("Failed to open directory: " + abs.string() + ": " + std::strerror(errno));
  }

  fstree::wait_group wg;

  // Subdirectories are read by tasks that refer to this frame and to the
  // iterator, so they must finish before an exception leaves the function
  wait_guard guard(wg);

  // Entries of the directory, added to the shared list together once the
  // directory has been read, rather than taking its lock for each of them
  std::vector<inode::ptr> entries;

  // Iterate the directory
  const struct dirent* entry;
  while ((entry = readdir(dir.get())) != nullptr) {
    // Get the path
    std::filesystem::path relpath = rel / entry->d_name;
    std::filesystem::path abspath = abs / entry->d_name;

    // Skip . and ..
    if (relpath.filename() == "." || relpath.filename() == ".." || relpath.filename() == ".fstree") {
      continue;
    }

    // Skip ignored directories
    if (entry->d_type == DT_DIR && ignores.match_entry(relpath.string(), true)) {
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
      std::error_code ec;
      target = std::filesystem::read_symlink(abspath, ec);
      if (ec) {
        // Removed or replaced since it was listed, as when lstat fails
        if (ec == std::errc::no_such_file_or_directory || ec == std::errc::invalid_argument) {
          continue;
        }
        throw std::filesystem::filesystem_error("read_symlink", abspath, ec);
      }
    }

    // convert mtime to uint64_t
#ifdef __APPLE__
    inode::time_type mtime = uint64_t(st.st_mtimespec.tv_sec) * 1000000000 + st.st_mtimespec.tv_nsec;
#else
    inode::time_type mtime = uint64_t(st.st_mtim.tv_sec) * 1000000000 + st.st_mtim.tv_nsec;
#endif

    // build status bits
    uint32_t status_bits = st.st_mode & (S_IRWXU|S_IRWXG|S_IRWXO);
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
    file_status status = normalize_permissions(file_status(status_bits), _permissions);

    // Add the path to the list of inodes
    // Only this call adds children to the parent, so that needs no lock
    inode::ptr node = fstree::make_intrusive<inode>(relpath.string(), status, mtime, st.st_size, target);
    parent->add_child(node);

    // An ignored file remains a child of its directory, marked as ignored,
    // but is left out of the list. Matching it here rather than after the
    // walk spreads the matching over the threads reading directories.
    if (entry->d_type != DT_DIR && ignores.match_entry(node->path(), false)) {
      node->ignore();
      continue;
    }

    entries.push_back(node);

    // Recurse if it's a directory
    if (_recursive && entry->d_type == DT_DIR) {
      wg.add(1);
      try {
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
      catch (...) {
        // Not queued, or the guard would wait for it forever
        wg.done();
        throw;
      }
    }
  }

  // Close the directory
  dir.reset();

  {
    std::lock_guard<std::mutex> lock(_mutex);
    _inodes.insert(_inodes.end(), entries.begin(), entries.end());
  }

  // Wait for all the children to finish
  wg.wait_rethrow();
}

}  // namespace fstree

#endif  // _WIN32
