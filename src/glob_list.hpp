#ifndef IGNORE_HPP
#define IGNORE_HPP

#include <filesystem>
#include <regex>
#include <string>
#include <vector>

namespace fstree {

// A list of .gitignore style patterns that can be matched against
// filesystem paths. A pattern prefixed with ! is negated and re-includes
// paths that an earlier pattern ignored. To match a literal leading !,
// escape it as \!.
//
// Patterns are evaluated in the order they were added and the last pattern
// that matches a path decides: the path is ignored if that pattern was a
// normal one, and not ignored if it was negated. A path that matches no
// pattern at all is not ignored.
//
// As in git, a path below an ignored directory cannot be re-included by a
// negated pattern. Directory traversal prunes ignored directories, so their
// contents are never visited in the first place.
class glob_list {
  // A pattern and the regex it compiles to, in the order it was added.
  struct rule {
    std::string pattern;
    bool negated = false;
    std::regex regex;
  };

  // Every rule, in the order added. Only consulted when the list actually
  // contains a negated pattern.
  std::vector<rule> _rules;

  // The non-negated patterns, in the order added. This is also the range
  // exposed by begin()/end().
  std::vector<std::string> _inclusive_patterns;

  // All non-negated patterns as a single alternation. Used on its own when
  // there are no negated patterns, and as a cheap pre-filter when there are.
  std::regex _inclusive_regex;

  // Whether any negated pattern has been added.
  bool _negations = false;

 public:
  glob_list();

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
