#ifndef IGNORE_HPP
#define IGNORE_HPP

#include <filesystem>
#include <string>
#include <vector>

namespace fstree {

// A list of patterns with the syntax and semantics of .gitignore, matched
// against relative paths separated by '/'.
//
// As in git, the last pattern that matches a path decides whether it is
// ignored, a pattern prefixed with ! re-includes what an earlier pattern
// ignored, and a path below an ignored directory is ignored, as git does not
// look inside such a directory, so a negated pattern cannot re-include it.
// Directory traversal prunes ignored directories for the same reason, so
// their contents are never visited in the first place.
//
// A pattern without a slash, other than a trailing one, matches the name of a
// file or directory at any level. Any other pattern matches the whole path
// from the root. A trailing slash matches directories only. Patterns may use
// *, ? and [...] as in git, and ** as a whole path component to match any
// number of directories.
class glob_list {
  // A parsed pattern, in the order it was added.
  struct rule {
    // The pattern to match, without !, leading or trailing slash
    std::string pattern;
    bool negated = false;

    // Whether the pattern only matches directories
    bool dir_only = false;

    // Whether the pattern matches the last component of a path, rather than
    // the whole path
    bool basename_only = false;
  };

  // Every rule, in the order added.
  std::vector<rule> _rules;

  // The non-negated patterns, in the order added, as given but without
  // trailing slashes. This is the range exposed by begin()/end().
  std::vector<std::string> _inclusive_patterns;

 public:
  glob_list();

  // Adds a line of a .gitignore file. Blank lines and comments are skipped.
  void add(const std::string& line);

  // Load patterns from a file in the format of .gitignore
  void load(const std::filesystem::path& path);

  // Kept for compatibility, the patterns need no further preparation.
  void finalize();

  // Returns true if the path, a file or a directory, should be ignored: if
  // one of its parent directories is ignored, or else if the last pattern
  // matching it is not negated. This is what git check-ignore reports.
  bool match(const std::string& path, bool is_dir = false) const;

  // Returns true if the path should be ignored, given that none of its parent
  // directories is, as when walking a tree that skips ignored directories.
  bool match_entry(const std::string& path, bool is_dir) const;

  std::vector<std::string>::const_iterator begin() const;
  std::vector<std::string>::const_iterator end() const;
};

}  // namespace fstree

#endif  // IGNORE_HPP
