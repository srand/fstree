#pragma once

#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fstree {

// A lock file is a file that is created to indicate that a resource is in use.
class lock_file {
 public:
  class context {
   public:
    context(lock_file& lock, std::unique_lock<std::mutex>&& mutex_lock);
    ~context();

    context(const context&) = delete;
    context& operator=(const context&) = delete;
    context(context&& other) noexcept;
    context& operator=(context&& other);

   private:
    lock_file* _lock;
    std::unique_lock<std::mutex> _mutex_lock;
  };

  lock_file(const std::filesystem::path& path);
  ~lock_file();

  // Lock the file. If the file is already locked, this function will block until the file is unlocked.
  context lock();

  // Try to lock the file. If the file is already locked, return std::nullopt without blocking.
  std::optional<context> try_lock();

  void unlock();

 private:
  std::shared_ptr<std::mutex> _mutex;
  std::filesystem::path _path;
#ifdef _WIN32
  HANDLE _handle;
#else
  int _fd;
#endif
};

}  // namespace fstree