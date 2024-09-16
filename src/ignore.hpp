#ifndef IGNORE_HPP
#define IGNORE_HPP

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
  ignore_list() = default;

  // Add a .gitignore style pattern to the ignore list
  void add(const std::string& input_pattern) {
    std::string pattern = input_pattern;

    // Ignore trailing slashes
    while (!pattern.empty() && pattern.back() == '/') {
      pattern.pop_back();
    }

    if (pattern.empty()) {
      return;
    }

    if (pattern[0] == '!') {
      throw std::runtime_error("negated patterns are not supported");
      _exclusive_patterns.push_back(pattern.substr(1));
    }
    else {
      _inclusive_patterns.push_back(pattern);
    }
  }

  void compile(const std::vector<std::string>& patterns, std::regex& regex) {
    std::string pattern;
    for (const auto& p : patterns) {
      if (!pattern.empty()) {
        pattern += "|";
      }
      else {
        pattern += "^";
      }
      pattern += "(";
      bool star = false;
      bool skip_slash = false;
      for (size_t i = 0; i < p.size(); ++i) {
        char c = p[i];

        if (i == 0) {
          if (c == '/') {
            pattern += "^";
            continue;
          }
          else {
            pattern += "(.*/)?";
          }
        }

        if (skip_slash) {
          skip_slash = false;
          if (c == '/') {
            continue;
          }
        }

        if (star) {
          if (c == '*') {
            pattern += "([^/]*(/[^/])*)(/?)";
            star = false;
            skip_slash = true;
            continue;
          }
          else {
            pattern += "[^/]*";
            star = false;
          }
        }

        switch (c) {
          case '*':
            star = true;
            break;
          case '?':
            pattern += ".";
            break;
          case '.':
            pattern += "\\.";
            break;
          default:
            if (star) {
              pattern += "[^/]*";
              star = false;
            }
            pattern += c;
            break;
        }
      }

      if (star) {
        pattern += "[^/]*";
      }

      pattern += "(/.*)?)$";
    }

    regex = std::regex(pattern);
  }

  // Load patterns from a file
  void load(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
      throw std::runtime_error("failed to open " + path);
    }

    std::string line;
    while (std::getline(file, line)) {
      if (!line.empty() && line[0] != '#') {
        add(line);
      }
    }

    file.close();

    finalize();
  }

  void finalize() {
    compile(_inclusive_patterns, _inclusive_regex);
    compile(_exclusive_patterns, _exclusive_regex);
  }

  // Returns true if the path should be ignored.
  // Returns false otherwise.
  bool match(const std::string& path) const {
    if (std::regex_match(path, _exclusive_regex)) {
      return false;
    }
    if (std::regex_match(path, _inclusive_regex)) {
      return true;
    }
    return false;
  }
};

}  // namespace fstree

#endif  // IGNORE_HPP
