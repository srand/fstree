#include "filesystem.hpp"

#include <cstring>

#include <sys/stat.h>
#include <unistd.h>

namespace fstree {

void lstat(const std::filesystem::path& path, stat& status_out) {
  // Stat new file and update inode
  struct ::stat st;

  int ret = ::lstat(path.c_str(), &st);
  if (ret != 0) {
    throw std::runtime_error("failed to stat file: " + path.string() + ": " + std::strerror(errno));
  }

  uint32_t status = st.st_mode & ACCESSPERMS;
  switch (st.st_mode & S_IFMT) {
    case S_IFDIR:
      status |= file_status::directory;
      break;
    case S_IFREG:
      status |= file_status::regular;
      break;
    case S_IFLNK:
      status |= file_status::symlink;
      break;
  }

  uint64_t mtime = uint64_t(st.st_mtim.tv_sec) * 1000000000 + st.st_mtim.tv_nsec;
  status_out.last_write_time = inode::time_type(std::chrono::nanoseconds(mtime));
  status_out.status = file_status(status);
}

FILE* mkstemp(std::filesystem::path& path) {
  std::string temp_path = path.string() + "/XXXXXXXXXX";

  // Create a temporary file
  int fd = ::mkstemp(temp_path.data());
  if (fd == -1) {
    return NULL;
  }

  path = std::filesystem::path(temp_path);

  return ::fdopen(fd, "w");
}

std::filesystem::path home_path() {
  std::filesystem::path home = getenv("HOME") ? getenv("HOME") : "";
  return home;
}

std::filesystem::path cache_path() {
  std::filesystem::path cache = home_path();
  if (!cache.empty()) cache /= ".cache/fstree";
  return cache;
}

}  // namespace fstree
