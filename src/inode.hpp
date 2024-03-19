#pragma once

#include "sha1.hpp"
#include "status.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace fstree {

class inode {
 public:
  using time_type = fs::file_time_type;

  // Constructor
  inode() : _status(file_status(std::filesystem::file_type::directory, std::filesystem::perms::none)) {}

  // Constructor
  inode(
      const std::string& path,
      file_status status,
      time_type mtime,
      const std::string& target,
      const std::string& hash = "")
      : _path(path), _hash(hash), _status(status), _last_write_time(mtime), _target(target) {}

  void add_child(inode* child) {
    _children.push_back(child);
    child->set_parent(this);
  }

  // Iterator begin
  std::vector<inode*>::iterator begin() { return _children.begin(); }

  // Iterator end
  std::vector<inode*>::iterator end() { return _children.end(); }

  // Const iterator begin
  std::vector<inode*>::const_iterator begin() const { return _children.begin(); }

  // Const iterator end
  std::vector<inode*>::const_iterator end() const { return _children.end(); }

  // Returns true if this inode is a directory
  bool is_directory() const { return _status.is_directory(); }

  // Returns true if this inode is a regular file
  bool is_file() const { return _status.is_regular(); }

  // Returns true if this inode is a symlink
  bool is_symlink() const { return _status.is_symlink(); }

  // Returns the sha1 hash of the file
  const std::string& hash() const { return _hash; }

  // Sets the sha1 hash of the file
  void set_hash(const std::string& hash) { _hash = hash; }

  // Return the file status, which includes the type and permissions
  file_status status() const { return _status; }

  // Set the file status, which includes the type and permissions
  void set_status(file_status status) { _status = status; }

  // Return the inode type
  std::filesystem::file_type type() const { return _status.type(); }

  // Return the inode permissions
  std::filesystem::perms permissions() const { return _status.permissions(); }

  // Return the last modification time
  time_type last_write_time() const { return _last_write_time; }

  void set_last_write_time(time_type last_write_time) { _last_write_time = last_write_time; }

  const std::string& target() const { return _target; }

  const std::string& path() const { return _path; }

  std::string name() const { return std::filesystem::path(_path).filename().string(); }

  void set_parent(inode* parent) { _parent = parent; }

  void set_dirty() {
    _hash.clear();
    if (_parent && !_parent->is_dirty()) {
      _parent->set_dirty();
    }
  }

  void sort() {
    std::sort(_children.begin(), _children.end(), [](const inode* a, const inode* b) { return a->path() < b->path(); });
  }

  bool is_dirty() const { return _hash.empty(); }

  void rehash(const std::filesystem::path& root) { _hash = sha1_hex_file(root / _path); }

  // equality operator
  bool operator==(const inode& other) const {
    return _path == other._path && _status == other._status && _last_write_time == other._last_write_time &&
           _target == other._target;
  }

  void ignore() { _ignored = true; }
  bool is_ignored() const { return _ignored; }

  void unignore() {
    if (!_unignored) {
      _unignored = true;
      if (_parent) _parent->unignore();
    }
  }
  bool is_unignored() const { return _unignored; }

 private:
  // The name of the file
  std::string _path;

  // The hash of the file
  std::string _hash;

  // The permissions of the file
  file_status _status;

  // The modification time of the file
  time_type _last_write_time;

  // Symlink target if this is a symlink
  std::string _target;

  // Children inodes if this is a directory
  std::vector<inode*> _children;

  // The parent inode if this is a child inode
  inode* _parent = nullptr;

  // If the inode was ignored
  bool _ignored = false;

  // If the inode was unignored (explicitly included)
  bool _unignored = false;
};

std::ostream& operator<<(std::ostream& os, const inode& inode);
std::istream& operator>>(std::istream& is, inode& inode);

}  // namespace fstree
