#pragma once

#include "intrusive_ptr.hpp"
#include "status.hpp"

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace fstree {

class inode : public intrusive_ptr_base<inode> {
 public:
  using time_type = std::chrono::time_point<std::chrono::nanoseconds>::rep;
  using ptr = intrusive_ptr<inode>;

  // Constructor
  inode();

  // Constructor
  inode(
      const std::string& path,
      file_status status,
      time_type mtime,
      size_t size,
      const std::string& target,
      const std::string& hash = "");

  void add_child(inode::ptr& child);

  // Iterator begin
  std::vector<inode::ptr>::iterator begin();

  // Iterator end
  std::vector<inode::ptr>::iterator end();

  // Const iterator begin
  std::vector<inode::ptr>::const_iterator begin() const;

  // Const iterator end
  std::vector<inode::ptr>::const_iterator end() const;

  // Returns true if this inode is a directory
  bool is_directory() const;

  // Returns true if this inode is a regular file
  bool is_file() const;

  // Returns true if this inode is a symlink
  bool is_symlink() const;

  // Returns the sha1 hash of the file
  const std::string& hash() const;

  // Sets the sha1 hash of the file
  void set_hash(const std::string& hash);

  // Return the file status, which includes the type and permissions
  file_status status() const;

  // Set the file status, which includes the type and permissions
  void set_status(file_status status);

  // Return the size of the file
  size_t size() const;

  // Return the inode type
  std::filesystem::file_type type() const;

  // Return the inode permissions
  std::filesystem::perms permissions() const;

  // Return the last modification time
  time_type last_write_time() const;

  void set_last_write_time(time_type last_write_time);

  const std::string& target() const;

  std::filesystem::path target_path() const;

  const std::string& path() const;

  std::string name() const;

  const inode::ptr& parent() const;

  void set_parent(const inode::ptr& parent);

  void set_dirty();

  void sort();

  bool is_dirty() const;

  bool is_equivalent(const inode::ptr& other) const;

  void rehash(const std::filesystem::path& root);

  // equality operator
  bool operator==(const inode& other) const;

  void ignore();
  bool is_ignored() const;

  void unignore();
  bool is_unignored() const;

  void clear();

 private:
  // The name of the file
  std::string _path;

  // The hash of the file
  std::string _hash;

  // The permissions of the file
  file_status _status;

  // The modification time of the file
  time_type _last_write_time;

  // The size of the file
  size_t _size = 0;

  // Symlink target if this is a symlink
  std::string _target;

  // Children inodes if this is a directory
  std::vector<inode::ptr> _children;

  // The parent inode if this is a child inode
  inode::ptr _parent = nullptr;

  // If the inode was ignored
  bool _ignored = false;

  // If the inode was unignored (explicitly included)
  bool _unignored = false;
};

std::ostream& operator<<(std::ostream& os, const inode& inode);
std::istream& operator>>(std::istream& is, inode& inode);

}  // namespace fstree
