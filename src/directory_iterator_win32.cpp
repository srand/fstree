#include "directory_iterator.hpp"
#include "thread_pool.hpp"
#include "wait_group.hpp"

#include <iostream>

#include <Windows.h>

namespace fs = std::filesystem;

namespace fstree {

// Iterator methods
std::vector<inode::ptr>::iterator sorted_directory_iterator::begin() { return _inodes.begin(); }
std::vector<inode::ptr>::iterator sorted_directory_iterator::end() { return _inodes.end(); }

inode::ptr& sorted_directory_iterator::root() { return _root; }
const inode::ptr& sorted_directory_iterator::root() const { return _root; }

void sorted_directory_iterator::read_directory(
    const std::filesystem::path& abs, const std::filesystem::path& rel, inode::ptr& parent, const glob_list& ignores) {
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

    fstree::inode::time_type mtime;
    mtime = result.ftLastWriteTime.dwHighDateTime;
    mtime <<= 32;
    mtime |= result.ftLastWriteTime.dwLowDateTime;
    mtime -= 116444736000000000LL;
    mtime *= 100;

    size_t size = result.nFileSizeHigh;
    size <<= 32;
    size |= result.nFileSizeLow;

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

    inode::ptr node = fstree::make_intrusive<fstree::inode>(path.string(), status, mtime, size, target.string());
    {
      std::lock_guard<std::mutex> lock(_mutex);
      _inodes.push_back(node);
      parent->add_child(node);
    }

    // Recurse if it's a directory
    if (_recursive && type == fs::file_type::directory) {
      wg.add(1);
      _pool->enqueue_or_run([this, abs, name, path, node, ignores, &wg] {
        try {
          inode::ptr node_c = intrusive_ptr<inode>(node);
          read_directory(abs / name, path, node_c, ignores);
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
