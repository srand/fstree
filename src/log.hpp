#pragma once

#include "mutex_ostream.hpp"

#include <ostream>

namespace fstree {

enum class log_level {
  debug = 0,
  info = 1,
  warn = 2,
  error = 3,
  off = 4,
};

// Set log level
void set_log_level(log_level level);

// Returns the log stream if enabled, otherwise returns a null stream
mutex_ostream log(log_level level = log_level::info);

}  // namespace fstree
