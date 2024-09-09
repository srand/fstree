#ifndef FILESYSTEM_HPP
#define FILESYSTEM_HPP

#include "inode.hpp"

#include <filesystem>

namespace fstree {

struct stat {
  fstree::inode::time_type last_write_time;
  fstree::file_status status;
};

std::filesystem::path cache_path();
std::filesystem::path home_path();
void lstat(const std::filesystem::path& path, stat& st);
FILE* mkstemp(std::filesystem::path& templ);
bool touch(const std::filesystem::path& path);

}  // namespace fstree

#endif  // FILESYSTEM_HPP
