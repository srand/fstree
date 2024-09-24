#include "filesystem.hpp"

#include <atomic>

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
    std::error_code ec(error, std::system_category());
    throw std::runtime_error("failed to stat file: " + path.string() + ": " + ec.message());
  }

  st.last_write_time = result.ftLastWriteTime.dwHighDateTime;
  st.last_write_time <<= 32;
  st.last_write_time |= result.ftLastWriteTime.dwLowDateTime;
  st.last_write_time -= 116444736000000000ULL;
  st.last_write_time *= 100;

  std::filesystem::perms perms;
  if (result.dwFileAttributes & FILE_ATTRIBUTE_READONLY) {
    perms = std::filesystem::perms::_File_attribute_readonly;
  }
  else {
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

// Get pid
std::atomic<int> pid = 0;

static int getpid() {
  if (pid == 0) {
    pid = GetCurrentProcessId();
  }
  return pid;
}

FILE* mkstemp(std::filesystem::path& path) {
  static std::atomic<int> counter;
  std::string pid = std::to_string(getpid());
  FILE* fp = nullptr;

  for (int i = 0; i < 59; i++) {
    int count = ++counter;
    std::string temp_path = path.string() + "\\" + pid + "-" + std::to_string(count);
    fp = fopen(temp_path.c_str(), "wbx");
    if (fp) {
      path = temp_path;
      break;
    }
  }

  return fp;
}

bool touch(const std::filesystem::path& path) {
  OFSTRUCT of;

  HANDLE handle =
      CreateFile(path.string().c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (handle == INVALID_HANDLE_VALUE) {
    return false;
  }

  FILETIME ft;
  GetSystemTimeAsFileTime(&ft);

  if (!SetFileTime(handle, nullptr, nullptr, &ft)) {
    CloseHandle(handle);
    throw std::runtime_error("failed to set file time");
  }

  CloseHandle(handle);
  return true;
}

}  // namespace fstree
