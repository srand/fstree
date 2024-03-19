#include "directory_iterator.hpp"

#include <iostream>

#include <Windows.h>

namespace fs = std::filesystem;

namespace fstree {

void sorted_recursive_directory_iterator::read_directory(
    const std::filesystem::path& abs, const std::filesystem::path& rel, inode* parent, std::error_code& ec) {
  fstree::wait_group wg;

  DWORD error;
  WIN32_FIND_DATAA result;
  HANDLE handle;

  // Iterate directory contents and create inodes
  handle = FindFirstFileA((abs.string() + "\\*").c_str(), &result);
  if (INVALID_HANDLE_VALUE == handle) {
    error = GetLastError();
    ec = std::error_code(error, std::system_category());
    return;
  }

  do {
    std::string name = result.cFileName;
    std::filesystem::path path = rel / name;
    std::filesystem::path path_abs = abs / name;

    // Skip . and ..
    if (name == "." || name == ".." || name == ".fstree") {
      continue;
    }

    // Get file type
    fs::file_type type;
    if (result.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      type = fs::file_type::directory;
    }
    else if (result.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
      type = fs::file_type::symlink;
    }
    else {
      type = fs::file_type::regular;
    }

    // Get permissions
    fs::perms perms = fs::perms::all;
    if (result.dwFileAttributes & FILE_ATTRIBUTE_READONLY) {
      perms &= ~fs::perms::owner_write;
      perms &= ~fs::perms::group_write;
      perms &= ~fs::perms::others_write;
    }

    // Get modification time
    inode::time_type mtime = result.ftLastWriteTime.dwHighDateTime;
    mtime <<= 32;
    mtime |= result.ftLastWriteTime.dwLowDateTime;
    inode::time_type mtime_tp = inode::time_type(std::chrono::nanoseconds(mtime));

    // Status
    file_status status(type, perms);

    // Get the target of the symlink
    fs::path target;
    if (type == fs::file_type::symlink) {
      target = fs::read_symlink(abs / name, ec);
      if (ec) {
        break;
      }
    }

    inode* child = new inode(path.string(), status, mtime_tp, target.string());
    {
      std::lock_guard<std::mutex> lock(_mutex);
      _inodes.push_back(child);
      parent->add_child(child);
    }

    // Recurse if it's a directory
    if (type == fs::file_type::directory) {
      wg.add(1);
      _pool.enqueue_or_run([this, abs, name, path, child, &wg] {
        std::error_code ec;
        read_directory(abs / name, path, child, ec);
        if (ec) {
          wg.error(ec);
        }
        else {
          wg.done();
        }
      });
    }
  } while (FindNextFileA(handle, &result));

  FindClose(handle);

  wg.wait();
  if (!ec) {
    ec = wg.error();
  }
}

}  // namespace fstree
