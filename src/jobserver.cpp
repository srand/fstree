#include "jobserver.hpp"

#include <cstdlib>
#include <string>

#if !defined(_WIN32)
#include <cerrno>
#include <cstdio>

#include <fcntl.h>
#include <unistd.h>
#endif

namespace fstree {

std::string jobserver::jobserver_auth_from_env() {
  const char* makeflags = std::getenv("MAKEFLAGS");
  if (makeflags == nullptr) {
    return "";
  }

  static const char* const keys[] = {"--jobserver-auth=", "--jobserver-fds="};

  std::string flags(makeflags);
  for (const char* key : keys) {
    std::string::size_type pos = flags.find(key);
    if (pos == std::string::npos) {
      continue;
    }
    pos += std::string(key).size();
    std::string::size_type end = flags.find_first_of(" \t", pos);
    return flags.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
  }

  return "";
}

jobserver::ptr jobserver::create() {
  std::string auth = jobserver_auth_from_env();
  if (auth.empty()) {
    return nullptr;
  }
  jobserver::ptr js(new jobserver(auth));
  if (!js->active()) {
    return nullptr;
  }
  return js;
}

jobserver::jobserver(const std::string& auth) {
#if !defined(_WIN32)
  // make >= 4.4 named-fifo form: "fifo:PATH".
  if (auth.compare(0, 5, "fifo:") == 0) {
    std::string path = auth.substr(5);
    int fd = ::open(path.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
      return;
    }
    _read_fd = fd;
    _write_fd = fd;
    _close_fds = true;
    _active = true;
    return;
  }

  // Legacy inherited pipe form: "R,W" file descriptors.
  int r = -1;
  int w = -1;
  if (std::sscanf(auth.c_str(), "%d,%d", &r, &w) != 2 || r < 0 || w < 0) {
    return;
  }
  // The fds are only valid if make actually shared them with this recipe.
  if (::fcntl(r, F_GETFD) == -1 || ::fcntl(w, F_GETFD) == -1) {
    return;
  }
  int flags = ::fcntl(r, F_GETFL);
  if (flags != -1) {
    ::fcntl(r, F_SETFL, flags | O_NONBLOCK);
  }
  _read_fd = r;
  _write_fd = w;
  _close_fds = false;
  _active = true;
#else
  (void)auth;
#endif
}

jobserver::~jobserver() {
#if !defined(_WIN32)
  if (_close_fds && _read_fd >= 0) {
    ::close(_read_fd);  // read_fd == write_fd for the fifo form
  }
#endif
}

bool jobserver::try_acquire(char& token) {
#if !defined(_WIN32)
  if (!_active) {
    return false;
  }
  for (;;) {
    ssize_t n = ::read(_read_fd, &token, 1);
    if (n == 1) {
      return true;
    }
    if (n < 0 && errno == EINTR) {
      continue;
    }
    // EOF, EAGAIN/EWOULDBLOCK or error: no token available right now.
    return false;
  }
#else
  (void)token;
  return false;
#endif
}

void jobserver::release(char token) {
#if !defined(_WIN32)
  if (!_active) {
    return;
  }
  for (;;) {
    ssize_t n = ::write(_write_fd, &token, 1);
    if (n == 1) {
      return;
    }
    if (n < 0 && errno == EINTR) {
      continue;
    }
    return;
  }
#else
  (void)token;
#endif
}

}  // namespace fstree
