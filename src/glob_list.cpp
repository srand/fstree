#include "glob_list.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace fstree {

namespace {

// Translate one .gitignore style pattern into a parenthesized regex fragment
// that matches the pattern itself and, when it names a directory, everything
// below it.
std::string compile_pattern(const std::string& p) {
  std::string pattern = "(";
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

  pattern += "(/.*)?)";
  return pattern;
}

}  // namespace

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

  bool negated = false;
  if (pattern[0] == '!') {
    negated = true;
    pattern.erase(0, 1);
  }
  else if (pattern[0] == '\\' && pattern.size() > 1 && pattern[1] == '!') {
    // A leading ! can be escaped to match it literally.
    pattern.erase(0, 1);
  }

  // A pattern that was nothing but ! selects nothing.
  if (pattern.empty()) {
    return;
  }

  if (negated) {
    _negations = true;
  }
  else {
    _inclusive_patterns.push_back(pattern);
  }

  rule r;
  r.pattern = pattern;
  r.negated = negated;
  _rules.push_back(std::move(r));
}

void glob_list::compile(const std::vector<std::string>& patterns, std::regex& regex) {
  std::string pattern;
  for (const auto& p : patterns) {
    if (!pattern.empty()) {
      pattern += "|";
    }
    pattern += compile_pattern(p);
  }

  regex = std::regex("^(" + pattern + ")$");
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

  // The per-pattern regexes are only needed to resolve the order in which
  // negations and normal patterns override each other. Without a negation
  // the combined regex above answers on its own.
  if (_negations) {
    for (auto& r : _rules) {
      r.regex = std::regex("^" + compile_pattern(r.pattern) + "$");
    }
  }
}

// Returns true if the path should be ignored.
bool glob_list::match(const std::string& path) const {
  if (_inclusive_patterns.empty()) {
    return false;
  }

  std::string adjusted_path = path;
#ifdef _WIN32
  for (auto& c : adjusted_path) {
    if (c == '\\') {
      c = '/';
    }
  }
#endif

  // Only a non-negated pattern can ignore a path, so a path matching none of
  // them is never ignored. This keeps the common case at a single regex and
  // spares the ordered scan below for every path that is not ignored anyway.
  if (!std::regex_match(adjusted_path, _inclusive_regex)) {
    return false;
  }

  if (!_negations) {
    return true;
  }

  // The last pattern that matches decides, so scan back to front and stop at
  // the first hit.
  for (auto it = _rules.rbegin(); it != _rules.rend(); ++it) {
    if (std::regex_match(adjusted_path, it->regex)) {
      return !it->negated;
    }
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
