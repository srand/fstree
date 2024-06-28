#include "log.hpp"

#include <iostream>
#include <mutex>
#include <ostream>
#include <string>

namespace fstree {

static log_level _log_level = log_level::off;
static std::ostream& _stream(std::cerr);
static std::mutex _mutex;

void set_log_level(log_level level) { _log_level = level; }

// Returns the log stream if enabled, otherwise returns a null stream
mutex_ostream log(log_level level) {
  mutex_ostream stream(_stream, _mutex);
  if (level >= _log_level) {
    switch (level) {
      case log_level::debug:
        stream << "debug - ";
        break;
      case log_level::info:
        stream << "info - ";
        break;
      case log_level::warn:
        stream << "warn - ";
        break;
      case log_level::error:
        stream << "error - ";
        break;
      default:
        break;
    }
    return stream;
  }

  return mutex_ostream();
}

}  // namespace fstree
