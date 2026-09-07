#ifndef JOBSERVER_HPP
#define JOBSERVER_HPP

#include "intrusive_ptr.hpp"

#include <string>

namespace fstree {

// Client for the GNU Make jobserver protocol. When fstree runs as part of a
// parallel `make -jN` build the jobserver hands out a fixed pool of tokens; a
// task may only run in parallel while it holds a token, which keeps the total
// number of concurrent jobs across the whole build within the -j limit.
// When no jobserver is advertised (or on unsupported platforms) active()
// returns false and callers should fall back to their own limiting.
class jobserver : public intrusive_ptr_base<jobserver> {
 public:
  using ptr = intrusive_ptr<jobserver>;

  ~jobserver();

  jobserver(const jobserver&) = delete;
  jobserver& operator=(const jobserver&) = delete;

  // Create a jobserver client from the configured path, or from the current
  // environment when no path is set. Returns null when no usable jobserver is
  // available.
  static ptr create();

  // Set the path of a jobserver named fifo to use instead of the jobserver
  // advertised in MAKEFLAGS. An empty path restores the environment lookup.
  static void set_path(const std::string& path);

  // Parse the GNU Make jobserver auth string from the MAKEFLAGS environment
  // variable. Returns the value of --jobserver-auth= (make >= 4.2) or the older
  // --jobserver-fds= flag, or an empty string if no jobserver is advertised.
  static std::string jobserver_auth_from_env();

  // True if a usable jobserver was found in the environment.
  bool active() const { return _active; }

  // Try to acquire a token without blocking. On success stores the token byte
  // (which must be passed back to release()) in `token` and returns true.
  bool try_acquire(char& token);

  // Return a previously acquired token to the jobserver.
  void release(char token);

 private:
  explicit jobserver(const std::string& auth);

  bool _active = false;
  int _read_fd = -1;
  int _write_fd = -1;
  bool _close_fds = false;
};

}  // namespace fstree

#endif  // JOBSERVER_HPP
