#ifndef MODE_HPP
#define MODE_HPP

#include <filesystem>
#include <string>

#include <sys/stat.h>

namespace fstree {

class file_status : public std::filesystem::file_status {
 public:
  enum internal_type { none = 0u, regular = 1u << 24, directory = 2u << 24, symlink = 4u << 24, mask = 7u << 24 };

  // Constructor
  explicit file_status(uint32_t m);

  // Constructor
  explicit file_status(std::filesystem::file_type type, std::filesystem::perms perms);

  // Constructor
  file_status();

  // Cast to unsigned int
  operator uint32_t() const;

  // Cast to bool
  operator bool() const;

  std::string str() const;

  bool is_regular() const;

  bool is_directory() const;

  bool is_symlink() const;
};

// How the permissions of files and directories are recorded in trees and
// restored at checkout. Only POSIX systems tell them apart.
enum class permission_mode {
  // All permission bits are recorded and restored as they were
  exact,

  // As git, only whether a file is executable is recorded. Files are recorded
  // as 0644 or 0755 and directories as 0755, and at checkout the umask
  // decides the rest, which makes trees the same whatever umask the files
  // were created with.
  git,
};

// Parses "exact" or "git", or throws std::invalid_argument
permission_mode parse_permission_mode(const std::string& mode);

// Returns the status to record for a file or directory found with the given
// status on disk.
file_status normalize_permissions(const file_status& status, permission_mode mode);

// Returns the permissions to give a file or directory checked out from a
// tree, where it was recorded with the given status.
std::filesystem::perms checkout_permissions(const file_status& status, permission_mode mode);

}  // namespace fstree

#endif  // MODE_HPP
