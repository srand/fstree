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

// Parse size string. Supports these suffixes:
// - K = 1000, M = 1000 * 1000, G = 1000 * 1000 * 1000, etc.
// - Ki = 1024, Mi = 1024 * 1024, Gi = 1024 * 1024 * 1024, etc.
// - KB = 1000, MB = 1000 * 1000, GB = 1000 * 1000 * 1000, etc.
// - KiB = 1024, MiB = 1024 * 1024, GiB = 1024 * 1024 * 1024, etc.
// Whitespace is allowed between the number and the unit
size_t parse_size(const std::string& size);

class argparser {
  struct option {
    std::string value;
    std::string default_value;
    bool has_value;

    // Whether the option may be given several times, collecting the values
    bool list = false;
    std::vector<std::string> values;

    // Whether the values were given on the command line, which then replace
    // those from the environment
    bool given = false;
  };

  // Sets the value of an option given on the command line
  void set_value(const std::string& name, const std::string& value);

  std::string _command;
  std::map<std::string, option*> _options;
  std::vector<std::string> _values;
  std::string _env_prefix;

 public:
  argparser();

  void parse(int argc, char** argv);

  void set_env_prefix(const std::string& prefix);

  void add_option(const std::string& name, const std::string& default_value);

  void add_option_alias(const std::string& name, const std::string& alias);

  void add_bool_option(const std::string& name);

  // Adds an option that may be given several times, collecting its values in
  // order. In the environment, its values are given one per line.
  void add_list_option(const std::string& name);

  std::string get_option(const std::string& name) const;

  // Returns the values of an option added with add_list_option
  std::vector<std::string> get_list_option(const std::string& name) const;

  std::filesystem::path get_option_path(const std::string& name, bool absolute = true) const;

  // Check if the option is present on command line
  bool has_option(const std::string& name) const;

  std::string get_value(size_t index) const;

  std::filesystem::path get_value_path(size_t index) const;

  // Returns the number of values
  size_t size() const;

  // Returns the value at the given index
  std::string operator[](size_t index) const;

  // Returns the command
  std::string command() const;
};

}  // namespace fstree

#endif  // ARGS_HPP
