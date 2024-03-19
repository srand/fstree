#include "filesystem.hpp"

#include <Windows.h>

namespace fstree {

std::filesystem::path home_path() {
  std::filesystem::path home = getenv("LOCALAPPDATA") ? getenv("LOCALAPPDATA") : "";
  return home;
}

std::filesystem::path cache_path() {
  std::filesystem::path cache = home_path();
  if (!cache.empty()) cache /= "fstree/cache";
  return cache;
}

void lstat(const std::filesystem::path& path, stat& st) {
  DWORD error;
  WIN32_FIND_DATAA result;

  HANDLE handle = FindFirstFileA(path.string().c_str(), &result);
  if (INVALID_HANDLE_VALUE == handle) {
    error = GetLastError();
    ec = std::error_code(error, std::system_category());
    throw std::runtime_error("failed to stat file: " + path.string() + ": " + ec.message());
  }

  st.ino = 0;
  st.last_write_time = result.ftLastWriteTime.dwHighDateTime;
  st.last_write_time <<= 32;
  st.last_write_time |= result.ftLastWriteTime.dwLowDateTime;

  if (result.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
    st.status = file_status::directory;
  }
  else if (result.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
    st.status = file_status::symlink;
  }
  else {
    st.status = file_status::regular;
  }

  FindClose(handle);
}

FILE* mkstemp(std::filesystem::path& path) {
  static std::atomic<int> counter;

  FILE* fp = nullptr;

  do {
    std::string temp_path;
    int count = ++counter;
    temp_path = path.string() + "/" + std::to_string(count);
    fp = fopen(temp_path.c_str(), "wx");
  } while (!fp);

  return fp;
}
}  // namespace fstree
