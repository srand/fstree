#pragma once

#include <stdexcept>
#include <string>

namespace fstree {

class exception : public std::runtime_error {
 public:
  explicit exception(const std::string& message) : std::runtime_error(message) {}
};

class unsupported_operation : public exception {
 public:
  explicit unsupported_operation(const std::string& message) : exception(message) {}
};

}  // namespace fstree
