#include "directory_iterator.hpp"

#include <iostream>

#include <Windows.h>

namespace fs = std::filesystem;

namespace fstree {

void sorted_recursive_directory_iterator::read_directory(
    const std::filesystem::path& abs, const std::filesystem::path& rel, inode* parent, const ignore_list& ignores) {
  fstree::wait_group wg;

  DWORD error;
  WIN32_FIND_DATAA result;
  HANDLE handle;

  // Iterate directory contents and create inodes
  handle = FindFirstFileA((abs.string() + "\\*").c_str(), &result);
  if (INVALID_HANDLE_VALUE == handle) {
    error = GetLastError();
    std::error_code ec(error, std::system_category());
    throw std::runtime_error("failed to traverse directory: " + rel.string() + ": " + ec.message());
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

    // Skip ignored directories
    if (type == fs::file_type::directory && ignores.match(path.string())) {
      continue;
    }

    // Get permissions
    fs::perms perms = fs::perms::all;
    if (result.dwFileAttributes & FILE_ATTRIBUTE_READONLY) {
      perms = fs::perms::_File_attribute_readonly;
    }

    fstree::inode::time_type time;
    time = result.ftLastWriteTime.dwHighDateTime;
    time <<= 32;
    time |= result.ftLastWriteTime.dwLowDateTime;
    time -= 116444736000000000LL;
    time *= 100;

    // Status
    file_status status(type, perms);

    // Get the target of the symlink
    fs::path target;
    if (type == fs::file_type::symlink) {
      std::error_code ec;
      target = fs::read_symlink(abs / name, ec);
      if (ec) {
        throw std::runtime_error("failed to read symlink: " + (abs / name).string() + ": " + ec.message());
      }
    }

    inode* child = new inode(path.string(), status, time, target.string());
    {
      std::lock_guard<std::mutex> lock(_mutex);
      _inodes.push_back(child);
      parent->add_child(child);
    }

    // Recurse if it's a directory
    if (type == fs::file_type::directory) {
      wg.add(1);
      _pool->enqueue_or_run([this, abs, name, path, child, ignores, &wg] {
        try {
          read_directory(abs / name, path, child, ignores);
          wg.done();
        }
        catch (const std::exception& e) {
          wg.exception(e);
        }
      });
    }
  } while (FindNextFileA(handle, &result));

  FindClose(handle);

  wg.wait_rethrow();
}

}  // namespace fstree
