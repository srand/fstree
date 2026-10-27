#ifndef _WIN32

#include "filesystem.hpp"

#include <atomic>
#include <cstring>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace fstree {

// POSIX filenames are opaque byte strings and std::filesystem::path stores them
// verbatim, so no conversion is needed or wanted here: a name that is not valid
// UTF-8 must survive the round trip unchanged.
std::filesystem::path to_path(const std::string& utf8) { return std::filesystem::path(utf8); }
std::string to_utf8(const std::filesystem::path& path) { return path.string(); }

void lstat(const std::filesystem::path& path, stat& status_out) {
  // Stat new file and update inode
  struct ::stat st;

  int ret = ::lstat(path.c_str(), &st);
  if (ret != 0) {
    throw std::runtime_error("failed to stat file: " + path.string() + ": " + std::strerror(errno));
  }

  uint32_t status = st.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO);
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

#ifdef __APPLE__
  inode::time_type mtime = uint64_t(st.st_mtimespec.tv_sec) * 1000000000 + st.st_mtimespec.tv_nsec;
#else
  inode::time_type mtime = uint64_t(st.st_mtim.tv_sec) * 1000000000 + st.st_mtim.tv_nsec;
#endif
  status_out.last_write_time = mtime;
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

FILE* mkstemp_umask(std::filesystem::path& path) {
  static std::atomic<int> counter;
  std::string pid = std::to_string(getpid());

  // ::mkstemp always creates the file with mode 0600. Opening it exclusively with
  // 0666 instead lets the kernel apply the umask, which can't be read without
  // briefly changing it for every thread.
  for (int i = 0; i < 59; i++) {
    std::string temp_path = path.string() + "/" + pid + "-" + std::to_string(++counter);
    int fd = ::open(temp_path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0666);
    if (fd != -1) {
      path = std::filesystem::path(temp_path);
      return ::fdopen(fd, "w");
    }
    if (errno != EEXIST) break;
  }

  return NULL;
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

bool touch(const std::filesystem::path& path) {
  // Update the access and modification time in a single call, rather than
  // opening the file to do it. The cache touches every object of a workspace
  // each time it is written. Fails, like opening, if the file does not exist.
  return utimensat(AT_FDCWD, path.c_str(), NULL, 0) == 0;
}

bool link_file(const std::filesystem::path& from, const std::filesystem::path& to) {
  if (::link(from.c_str(), to.c_str()) == 0) {
    return true;
  }

  if (errno == EEXIST) {
    return false;
  }

  throw std::runtime_error(
      "failed to link file: " + from.string() + " -> " + to.string() + ": " + std::strerror(errno));
}

}  // namespace fstree

#endif  // _WIN32
