#include "directory_iterator.hpp"

#include <iostream>

#include <Windows.h>

namespace fs = std::filesystem;

namespace fstree {

void sorted_directory_iterator::read_directory(
    const std::filesystem::path& abs, const std::filesystem::path& rel, inode* parent, std::error_code& ec) {
  fstree::wait_group wg;
  fs::directory_iterator it(abs, ec);

  for (const auto& entry : it) {
    // Get the path
    fs::path path = entry.path();
    fs::path name = path.filename();

    // Skip . and ..
    if (name == "." || name == ".." || name == ".fstree") {
      continue;
    }

    // Get inode number using GetFileInformationByHandle
    inode::ino_type ino = 0;

    // BY_HANDLE_FILE_INFORMATION info;
    //
    // if (!GetFileInformationByHandle(handle, &info)) {
    //  error = GetLastError();
    //  ec = std::error_code(error, std::system_category());
    //  break;
    //}
    //
    // ino = info.nFileIndexHigh;
    // ino <<= 32;
    // ino |= info.nFileIndexLow;

    // Get file status
    fs::file_status status = entry.status(ec);

    // Get file type
    fs::file_type type = fs::file_type::unknown;
    if (fs::is_directory(status)) {
      type = fs::file_type::directory;
    }
    else if (fs::is_symlink(status)) {
      type = fs::file_type::symlink;
    }
    else if (fs::is_regular_file(status)) {
      type = fs::file_type::regular;
    }

    // Get permissions
    fs::perms perms = status.permissions();

    // Get mtime
    uint64_t mtime = fs::last_write_time(path).time_since_epoch().count();
    uint64_t atime = mtime;
    size_t size = fs::file_size(path);

    // Get the target of the symlink
    fs::path target;
    if (type == fs::file_type::symlink) {
      target = fs::read_symlink(abs / name, ec);
      if (ec) {
        break;
      }
    }

    inode* child = new inode(path.string(), ino, file_status(type, perms), mtime, atime, size, target.string());
    {
      std::lock_guard<std::mutex> lock(_mutex);
      _inodes.push_back(child);
      parent->add_child(child);
    }

    // Recurse if it's a directory
    if (_recursive && type == fs::file_type::directory) {
      wg.add(1);
      _pool.enqueue_or_run([this, abs, name, path, child, &wg] {
        try {
          read_directory(abs / name, path, child);
          wg.done();
        }
        catch (const std::exception& e) {
          wg.exception(e);
        }
      });
    }
  }

  wg.wait_rethrow();
}

}  // namespace fstree
