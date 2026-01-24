#ifndef IGNORE_HPP
#define IGNORE_HPP

#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

namespace fstree {

// A list of .gitignore style patterns that can be matched against
// filesystem paths. The patterns can be negated by prefixing them with !.
// The patterns are matched in the order they are added.
// The first pattern that matches the path determines if the path is ignored.
// If a negated pattern matches the path, the path is not ignored.
class ignore_list {
  std::vector<std::string> _inclusive_patterns;
  std::vector<std::string> _exclusive_patterns;
  std::regex _inclusive_regex;
  std::regex _exclusive_regex;

 public:
  ignore_list();

  // Add a .gitignore style pattern to the ignore list
  void add(const std::string& input_pattern);

  void compile(const std::vector<std::string>& patterns, std::regex& regex);

  // Load patterns from a file
  void load(const std::filesystem::path& path);

  void finalize();

  // Returns true if the path should be ignored.
  // Returns false otherwise.
  bool match(const std::string& path) const;

  std::vector<std::string>::const_iterator begin() const;
  std::vector<std::string>::const_iterator end() const;
};

}  // namespace fstree

#endif  // IGNORE_HPP
