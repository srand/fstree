#ifndef FILESYSTEM_HPP
#define FILESYSTEM_HPP

#include "encoding.hpp"
#include "inode.hpp"

#include <cstdint>
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
// Like mkstemp, but the file gets the permissions the umask gives a newly created
// file rather than owner-only access.
FILE* mkstemp_umask(std::filesystem::path& templ);
bool touch(const std::filesystem::path& path);

// Returns the umask of the process, 0 on Windows. Unless it can be read from
// /proc, reading it changes it briefly for every thread, so it is read once,
// by the first call, which should come before threads create files.
uint32_t process_umask();
bool link_file(const std::filesystem::path& from, const std::filesystem::path& to);

}  // namespace fstree

#endif  // FILESYSTEM_HPP
