#include "filesystem.hpp"

#include <Windows.h>
#include <atomic>

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
    std::error_code ec(error, std::system_category());
    throw std::runtime_error("failed to stat file: " + path.string() + ": " + ec.message());
  }

  uint64_t time;
  time = result.ftLastWriteTime.dwHighDateTime;
  time <<= 32;
  time = result.ftLastWriteTime.dwLowDateTime;
  st.last_write_time = std::filesystem::file_time_type(std::chrono::microseconds(time));

  std::filesystem::perms perms;
  if (result.dwFileAttributes & FILE_ATTRIBUTE_READONLY) {
    perms = std::filesystem::perms::_File_attribute_readonly;
  } else {
    perms = std::filesystem::perms::all;
  }

  if (result.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
    st.status = file_status(std::filesystem::file_type::directory, perms);
  }
  else if (result.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
    st.status = file_status(std::filesystem::file_type::symlink, perms);
  }
  else {
    st.status = file_status(std::filesystem::file_type::regular, perms);
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
