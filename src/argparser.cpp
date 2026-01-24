#include "argparser.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstdlib>

namespace fstree {

size_t parse_size(const std::string& size) {
  // Parse size string. Supports these suffixes:
  // - K = 1000
  // - M = 1000 * 1000
  // - G = 1000 * 1000 * 1000
  // - T = 1000 * 1000 * 1000 * 1000
  // - Ki = 1024
  // - Mi = 1024 * 1024
  // - Gi = 1024 * 1024 * 1024
  // - Ti = 1024 * 1024 * 1024 * 1024
  // - KB = 1000
  // - MB = 1000 * 1000
  // - GB = 1000 * 1000 * 1000
  // - TB = 1000 * 1000 * 1000 * 1000
  // - KiB = 1024
  // - MiB = 1024 * 1024
  // - GiB = 1024 * 1024 * 1024
  // - TiB = 1024 * 1024 * 1024 * 1024
  // whitespace is allowed between the number and the unit

  // Remove all whitespace
  std::string s = size;
  s.erase(std::remove_if(s.begin(), s.end(), ::isspace), s.end());

  // Remove trailing 'B' from unit
  if (s.back() == 'B') {
    s.pop_back();
  }

  size_t number_length = s.find_first_not_of("0123456789");
  if (number_length == std::string::npos) {
    // No unit
    return std::stoull(s);
  }

  size_t number = std::stoull(s.substr(0, number_length));
  std::string unit = s.substr(number_length);
  if (unit == "K") {
    return number * 1000;
  }
  if (unit == "M") {
    return number * 1000 * 1000;
  }
  if (unit == "G") {
    return number * 1000 * 1000 * 1000;
  }
  if (unit == "T") {
    return number * 1000 * 1000 * 1000 * 1000;
  }
  if (unit == "Ki") {
    return number * 1024;
  }
  if (unit == "Mi") {
    return number * 1024 * 1024;
  }
  if (unit == "Gi") {
    return number * 1024 * 1024 * 1024;
  }
  if (unit == "Ti") {
    return number * 1024 * 1024 * 1024 * 1024;
  }

  throw std::invalid_argument("invalid size unit: " + unit);
}

argparser::argparser() {}

void argparser::parse(int argc, char** argv) {
  _command = argv[0];

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg[0] == '-') {
      // --option=value
      size_t pos = arg.find('=');
      if (pos != std::string::npos) {
        std::string value = arg.substr(pos + 1);
        if (_options.find(arg) == _options.end()) {
          throw std::invalid_argument("unknown option: " + arg);
        }
        if (!_options[arg]->has_value) {
          throw std::invalid_argument("option does not take a value: " + arg);
        }
        _options[arg]->value = value;
        continue;
      }

      // --option value
      if (_options.find(arg) == _options.end()) {
        throw std::invalid_argument("unknown option: " + arg);
      }

      if (_options[arg]->has_value) {
        if (i + 1 >= argc) {
          throw std::invalid_argument("missing value for option: " + arg);
        }
        _options[arg]->value = argv[++i];
      }
      else {
        _options[arg]->value = "true";
      }
    }
    else {
      _values.push_back(arg);
    }
  }
}

void argparser::set_env_prefix(const std::string& prefix) { 
  _env_prefix = prefix; 
}

void argparser::add_option(const std::string& name, const std::string& default_value) {
  _options[name] = new option{"", default_value, true};

  // Check if the option is set in the environment
  if (!_env_prefix.empty()) {
    // Replace - with _
    std::string env_name = _env_prefix + "_" + name.substr(2);
    std::replace(env_name.begin(), env_name.end(), '-', '_');

    // Convert to uppercase
    std::transform(env_name.begin(), env_name.end(), env_name.begin(), ::toupper);

    if (const char* env_value = std::getenv(env_name.c_str())) {
      _options[name]->value = env_value;
    }
  }
}

void argparser::add_option_alias(const std::string& name, const std::string& alias) {
  if (_options.find(name) == _options.end()) {
    throw std::invalid_argument("unknown option: " + name);
  }
  _options[alias] = _options[name];
}

void argparser::add_bool_option(const std::string& name) { 
  _options[name] = new option{"", "", false}; 
}

std::string argparser::get_option(const std::string& name) const {
  if (_options.find(name) == _options.end()) {
    throw std::invalid_argument("unknown option: " + name);
  }
  if (_options.at(name)->value.empty()) {
    return _options.at(name)->default_value;
  }
  return _options.at(name)->value;
}

std::filesystem::path argparser::get_option_path(const std::string& name, bool absolute) const {
  if (absolute) {
    return std::filesystem::absolute(get_option(name)).lexically_normal();
  }
  return std::filesystem::path(get_option(name)).lexically_normal();
}

bool argparser::has_option(const std::string& name) const {
  if (_options.find(name) == _options.end()) {
    throw std::invalid_argument("unknown option: " + name);
  }
  return !_options.at(name)->value.empty();
}

std::string argparser::get_value(size_t index) const {
  if (index >= _values.size()) {
    throw std::invalid_argument("index out of range");
  }
  return _values[index];
}

std::filesystem::path argparser::get_value_path(size_t index) const {
  return std::filesystem::absolute(get_value(index)).lexically_normal();
}

size_t argparser::size() const { 
  return _values.size(); 
}

std::string argparser::operator[](size_t index) const {
  if (index >= _values.size()) {
    throw std::invalid_argument("index out of range");
  }
  return _values[index];
}

std::string argparser::command() const { 
  return _command; 
}

}  // namespace fstree