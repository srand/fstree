#pragma once

#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fstree {

// A lock file is a file that is created to indicate that a resource is in use.
class lock_file {
 public:
  class context {
   public:
    context(lock_file& lock) : _lock(lock) {}
    ~context() { _lock.unlock(); }

   private:
    lock_file& _lock;
  };

  lock_file(const std::filesystem::path& path);
  ~lock_file();

  // Lock the file. If the file is already locked, this function will block until the file is unlocked.
  context lock();
  void unlock();

 private:
  std::filesystem::path _path;
#ifdef _WIN32
  HANDLE _handle;
#else
  int _fd;
#endif
};

}  // namespace fstree