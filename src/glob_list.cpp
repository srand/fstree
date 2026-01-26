#include "glob_list.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

namespace fstree {

// Default constructor
glob_list::glob_list() = default;

// Add a .gitignore style pattern to the ignore list
void glob_list::add(const std::string& input_pattern) {
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

void glob_list::compile(const std::vector<std::string>& patterns, std::regex& regex) {
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
void glob_list::load(const std::filesystem::path& path) {
  std::ifstream file(path);
  if (!file) {
    throw std::runtime_error("failed to open " + path.string() + " for reading");
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

void glob_list::finalize() {
  compile(_inclusive_patterns, _inclusive_regex);
  compile(_exclusive_patterns, _exclusive_regex);
}

// Returns true if the path should be ignored.
bool glob_list::match(const std::string& path) const {
  std::string adjusted_path = path;
#ifdef _WIN32
  for (auto& c : adjusted_path) {
    if (c == '\\') {
      c = '/';
    }
  }
#endif
  if (std::regex_match(adjusted_path, _exclusive_regex)) {
    return false;
  }
  if (std::regex_match(adjusted_path, _inclusive_regex)) {
    return true;
  }
  return false;
}

std::vector<std::string>::const_iterator glob_list::begin() const { 
  return _inclusive_patterns.begin(); 
}

std::vector<std::string>::const_iterator glob_list::end() const { 
  return _inclusive_patterns.end(); 
}

}  // namespace fstree
