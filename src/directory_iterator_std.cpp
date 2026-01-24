#include "directory_iterator.hpp"
#include "thread_pool.hpp"
#include "wait_group.hpp"

namespace fs = std::filesystem;

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
      const std::filesystem::path& abs, const std::filesystem::path& rel, inode::ptr& parent, const ignore_list& ignores) 
{
  std::error_code ec;
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
    size_t size = fs::file_size(path);

    // Get the target of the symlink
    fs::path target;
    if (type == fs::file_type::symlink) {
      target = fs::read_symlink(abs / name, ec);
      if (ec) {
        break;
      }
    }

    inode::ptr node = fstree::make_intrusive<inode>(path.string(), file_status(type, perms), mtime, size, target.string());
    {
      std::lock_guard<std::mutex> lock(_mutex);
      _inodes.push_back(node);
      parent->add_child(node);
    }

    // Recurse if it's a directory
    if (_recursive && type == fs::file_type::directory) {
      wg.add(1);
      _pool->enqueue_or_run([this, abs, name, path, node, &wg] {
        try {
          inode::ptr node_c = intrusive_ptr<inode>(node);
          read_directory(abs / name, path, node_c, _ignores);
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
