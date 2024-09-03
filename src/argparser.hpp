#ifndef ARGS_HPP
#define ARGS_HPP

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace fstree {

class argparser {
  struct option {
    std::string value;
    std::string default_value;
    bool has_value;
  };

  std::map<std::string, option*> _options;
  std::vector<std::string> _values;
  std::string _env_prefix;

 public:
  argparser() {}

  void parse(int argc, char** argv) {
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

  void set_env_prefix(const std::string& prefix) { _env_prefix = prefix; }

  void add_option(const std::string& name, const std::string& default_value) {
    _options[name] = new option{"", default_value, true};

    // Check if the option is set in the environment
    if (!_env_prefix.empty()) {
      std::string env_name = _env_prefix + "_" + name.substr(2);

      // Convert to uppercase
      std::transform(env_name.begin(), env_name.end(), env_name.begin(), ::toupper);

      if (const char* env_value = std::getenv(env_name.c_str())) {
        _options[name]->value = env_value;
      }
    }
  }

  void add_option_alias(const std::string& name, const std::string& alias) {
    if (_options.find(name) == _options.end()) {
      throw std::invalid_argument("unknown option: " + name);
    }
    _options[alias] = _options[name];
  }

  void add_bool_option(const std::string& name) { _options[name] = new option{"", "", false}; }

  std::string get_option(const std::string& name) const {
    if (_options.find(name) == _options.end()) {
      throw std::invalid_argument("unknown option: " + name);
    }
    if (_options.at(name)->value.empty()) {
      return _options.at(name)->default_value;
    }
    return _options.at(name)->value;
  }

  std::filesystem::path get_option_path(const std::string& name, bool absolute = true) const {
    if (absolute) {
      return std::filesystem::absolute(get_option(name)).lexically_normal();
    }
    return std::filesystem::path(get_option(name)).lexically_normal();
  }

  // Check if the option is present on command line
  bool has_option(const std::string& name) const {
    if (_options.find(name) == _options.end()) {
      throw std::invalid_argument("unknown option: " + name);
    }
    return !_options.at(name)->value.empty();
  }

  std::string get_value(size_t index) const {
    if (index >= _values.size()) {
      throw std::invalid_argument("index out of range");
    }
    return _values[index];
  }

  std::filesystem::path get_value_path(size_t index) const {
    return std::filesystem::absolute(get_value(index)).lexically_normal();
  }

  // Returns the number of values
  size_t size() const { return _values.size(); }

  // Returns the value at the given index
  std::string operator[](size_t index) const {
    if (index >= _values.size()) {
      throw std::invalid_argument("index out of range");
    }
    return _values[index];
  }
};

}  // namespace fstree

#endif  // ARGS_HPP
