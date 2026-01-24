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

}  // namespace fstree

#endif  // MODE_HPP
