#ifdef _WIN32

#include "filesystem.hpp"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

#include <Windows.h>

namespace fstree {

std::wstring to_wide(const std::string& utf8) {
  if (utf8.empty()) return std::wstring();

  int size = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
  if (size <= 0) {
    throw std::runtime_error("failed to convert path to UTF-16: " + utf8);
  }

  std::wstring utf16(static_cast<size_t>(size), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), utf16.data(), size);
  return utf16;
}

std::string from_wide(const std::wstring& utf16) {
  if (utf16.empty()) return std::string();

  int size =
      WideCharToMultiByte(CP_UTF8, 0, utf16.data(), static_cast<int>(utf16.size()), nullptr, 0, nullptr, nullptr);
  if (size <= 0) {
    throw std::runtime_error("failed to convert path to UTF-8");
  }

  std::string utf8(static_cast<size_t>(size), '\0');
  WideCharToMultiByte(CP_UTF8, 0, utf16.data(), static_cast<int>(utf16.size()), utf8.data(), size, nullptr, nullptr);
  return utf8;
}

// std::filesystem::path stores the native UTF-16 form on Windows, so its narrow
// conversions go through the active code page. Go through UTF-16 explicitly.
std::filesystem::path to_path(const std::string& utf8) { return std::filesystem::path(to_wide(utf8)); }
std::string to_utf8(const std::filesystem::path& path) { return from_wide(path.native()); }

std::filesystem::path home_path() {
  const wchar_t* local_appdata = _wgetenv(L"LOCALAPPDATA");
  std::filesystem::path home = local_appdata ? local_appdata : L"";
  return home;
}

std::filesystem::path cache_path() {
  std::filesystem::path cache = home_path();
  if (!cache.empty()) cache /= "fstree/cache";
  return cache;
}

void lstat(const std::filesystem::path& path, stat& st) {
  DWORD error;
  WIN32_FIND_DATAW result;

  HANDLE handle = FindFirstFileW(path.c_str(), &result);
  if (INVALID_HANDLE_VALUE == handle) {
    error = GetLastError();
    std::error_code ec(error, std::system_category());
    throw std::runtime_error("failed to stat file: " + to_utf8(path) + ": " + ec.message());
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
  std::wstring pid = std::to_wstring(getpid());
  FILE* fp = nullptr;

  for (int i = 0; i < 59; i++) {
    int count = ++counter;
    std::wstring temp_path = path.native() + L"\\" + pid + L"-" + std::to_wstring(count);
    fp = _wfopen(temp_path.c_str(), L"wbx");
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
      CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
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

bool link_file(const std::filesystem::path& from, const std::filesystem::path& to) {
  if (CreateHardLinkW(to.c_str(), from.c_str(), nullptr)) {
    return true;
  }

  DWORD error = GetLastError();
  if (error == ERROR_ALREADY_EXISTS || error == ERROR_FILE_EXISTS) {
    return false;
  }

  std::error_code ec(error, std::system_category());
  throw std::runtime_error("failed to link file: " + to_utf8(from) + " -> " + to_utf8(to) + ": " + ec.message());
}

}  // namespace fstree

#endif  // _WIN32
