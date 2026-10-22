#include "glob_list.hpp"

#include <cctype>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fstree {

namespace {

// Outcome of matching a pattern against text. Besides a match or not, a
// failure may also rule out every other way of matching, which ends the
// search early instead of trying all positions of every *.
enum class wildmatch_result {
  match,
  no_match,

  // No match is possible at all, for instance as the text ran out, so no
  // enclosing * needs to try matching more of the text.
  abort_all,

  // A * would have to match a '/' to continue, so only an enclosing **,
  // which can, needs to try matching more of the text.
  abort_to_double_star,
};

// Returns whether c belongs to the named character class of a bracket
// expression, such as "alpha" for [[:alpha:]], or throws if there is no
// such class.
bool in_class(const std::string& name, unsigned char c) {
  if (name == "alnum") return std::isalnum(c);
  if (name == "alpha") return std::isalpha(c);
  if (name == "blank") return c == ' ' || c == '\t';
  if (name == "cntrl") return std::iscntrl(c);
  if (name == "digit") return std::isdigit(c);
  if (name == "graph") return std::isgraph(c);
  if (name == "lower") return std::islower(c);
  if (name == "print") return std::isprint(c);
  if (name == "punct") return std::ispunct(c);
  if (name == "space") return std::isspace(c);
  if (name == "upper") return std::isupper(c);
  if (name == "xdigit") return std::isxdigit(c);
  throw std::invalid_argument(name);
}

// Matches the bracket expression starting at p, the character after '[',
// against c. On return, p points to the closing ']'. Returns abort_all if
// the expression is malformed, as git then matches nothing.
wildmatch_result match_bracket(const char*& p, unsigned char c) {
  bool negated = false;
  if (*p == '!' || *p == '^') {
    negated = true;
    p++;
  }

  bool matched = false;
  bool first = true;

  // The previous character, the start of a range if followed by '-'
  bool have_prev = false;
  unsigned char prev = 0;

  for (;; p++) {
    unsigned char pc = *p;
    if (pc == '\0') {
      return wildmatch_result::abort_all;
    }

    // A ']' first in the expression is literal, otherwise it closes it
    if (pc == ']' && !first) {
      break;
    }
    first = false;

    if (pc == '\\') {
      pc = *++p;
      if (pc == '\0') {
        return wildmatch_result::abort_all;
      }
    }
    else if (pc == '-' && have_prev && p[1] != '\0' && p[1] != ']') {
      unsigned char last = *++p;
      if (last == '\\') {
        last = *++p;
        if (last == '\0') {
          return wildmatch_result::abort_all;
        }
      }
      matched = matched || (c >= prev && c <= last);
      have_prev = false;
      continue;
    }
    else if (pc == '[' && p[1] == ':') {
      const char* name = p + 2;
      const char* close = std::strchr(name, ']');
      if (close == nullptr) {
        return wildmatch_result::abort_all;
      }
      if (close > name && close[-1] == ':') {
        try {
          matched = matched || in_class(std::string(name, close - 1), c);
        }
        catch (const std::invalid_argument&) {
          return wildmatch_result::abort_all;
        }
        p = close;
        have_prev = false;
        continue;
      }
      // Not a class name, so the '[' is literal
    }

    matched = matched || c == pc;
    prev = pc;
    have_prev = true;
  }

  return matched != negated ? wildmatch_result::match : wildmatch_result::no_match;
}

// Characters with a meaning of their own in a pattern
bool is_special(char c) { return c == '*' || c == '?' || c == '[' || c == '\\'; }

// Matches text against a pattern in which * and ? do not match '/', and **
// as a whole path component matches any number of directories. pattern is
// the start of the whole pattern, needed to tell where components begin.
wildmatch_result wildmatch(const char* p, const char* text, const char* pattern) {
  for (; *p != '\0'; p++, text++) {
    unsigned char pc = *p;
    unsigned char tc = *text;

    if (tc == '\0' && pc != '*') {
      return wildmatch_result::abort_all;
    }

    switch (pc) {
      case '\\':
        // The next character is literal. A trailing backslash makes the
        // pattern invalid, and it matches nothing.
        pc = *++p;
        if (pc == '\0' || tc != pc) {
          return wildmatch_result::no_match;
        }
        continue;

      case '?':
        if (tc == '/') {
          return wildmatch_result::no_match;
        }
        continue;

      case '[': {
        p++;
        wildmatch_result result = match_bracket(p, tc);
        if (result == wildmatch_result::abort_all) {
          return result;
        }
        if (result != wildmatch_result::match || tc == '/') {
          return wildmatch_result::no_match;
        }
        continue;
      }

      case '*': {
        // Consecutive stars are one, which matches across directories only
        // if it makes up a whole path component, such as a/**/b.
        const char* first = p;
        while (p[1] == '*') {
          p++;
        }

        bool any_directories = false;
        if (p > first && (first == pattern || first[-1] == '/') && (p[1] == '\0' || p[1] == '/')) {
          any_directories = true;

          // "**/" may also match no directory at all
          if (p[1] == '/' && wildmatch(p + 2, text, pattern) == wildmatch_result::match) {
            return wildmatch_result::match;
          }
        }

        const char* rest = p + 1;
        if (*rest == '\0') {
          // Trailing stars match the rest of the text, but a single one
          // only if no directory follows.
          if (!any_directories && std::strchr(text, '/') != nullptr) {
            return wildmatch_result::no_match;
          }
          return wildmatch_result::match;
        }

        for (;; text++) {
          // When the rest begins with a literal character, it can only
          // match where the text has it.
          if (!is_special(*rest)) {
            while (*text != '\0' && *text != *rest && (any_directories || *text != '/')) {
              text++;
            }
          }

          wildmatch_result result = wildmatch(rest, text, pattern);
          if (result == wildmatch_result::match || result == wildmatch_result::abort_all) {
            return result;
          }
          if (result == wildmatch_result::abort_to_double_star && !any_directories) {
            return result;
          }

          if (*text == '\0') {
            return wildmatch_result::abort_all;
          }
          if (*text == '/' && !any_directories) {
            return wildmatch_result::abort_to_double_star;
          }
        }
      }

      default:
        if (tc != pc) {
          return wildmatch_result::no_match;
        }
        continue;
    }
  }

  return *text == '\0' ? wildmatch_result::match : wildmatch_result::no_match;
}

bool wildmatch(const std::string& pattern, const char* text) {
  return wildmatch(pattern.c_str(), text, pattern.c_str()) == wildmatch_result::match;
}

// Removes trailing spaces, except one escaped with a backslash, as git does.
void trim_trailing_spaces(std::string& line) {
  size_t end = line.size();
  size_t first_space = std::string::npos;

  for (size_t i = 0; i < end; i++) {
    if (line[i] == ' ') {
      if (first_space == std::string::npos) {
        first_space = i;
      }
    }
    else {
      // An escaped character, space or not, ends a run of spaces
      if (line[i] == '\\' && i + 1 < end) {
        i++;
      }
      first_space = std::string::npos;
    }
  }

  if (first_space != std::string::npos) {
    line.resize(first_space);
  }
}

}  // namespace

// Default constructor
glob_list::glob_list() = default;

void glob_list::add(const std::string& input_line) {
  // The patterns iterated over by begin()/end() are kept as before: the line
  // without trailing slashes, unless negated.
  {
    std::string pattern = input_line;
    while (!pattern.empty() && pattern.back() == '/') {
      pattern.pop_back();
    }
    if (!pattern.empty() && pattern[0] != '!') {
      if (pattern[0] == '\\' && pattern.size() > 1 && pattern[1] == '!') {
        pattern.erase(0, 1);
      }
      _inclusive_patterns.push_back(pattern);
    }
  }

  std::string line = input_line;

  // A line may end with a carriage return, from a file with CRLF endings
  if (!line.empty() && line.back() == '\r') {
    line.pop_back();
  }

  // Blank lines and comments match nothing
  if (line.empty() || line[0] == '#') {
    return;
  }

  trim_trailing_spaces(line);

  rule r;
  std::string_view pattern = line;

  if (!pattern.empty() && pattern[0] == '!') {
    r.negated = true;
    pattern.remove_prefix(1);
  }

  if (!pattern.empty() && pattern.back() == '/') {
    r.dir_only = true;
    pattern.remove_suffix(1);
  }

  // Without a slash, the pattern matches a name at any level, otherwise the
  // path from the root
  r.basename_only = pattern.find('/') == std::string_view::npos;
  if (!pattern.empty() && pattern[0] == '/') {
    pattern.remove_prefix(1);
  }

  // Such as "/" or "!", which match nothing
  if (pattern.empty()) {
    return;
  }

  r.pattern = std::string(pattern);
  _rules.push_back(std::move(r));
}

// Load patterns from a file
void glob_list::load(const std::filesystem::path& path) {
  std::ifstream file(path);
  if (!file) {
    throw std::runtime_error("failed to open " + path.string() + " for reading");
  }

  std::string line;
  while (std::getline(file, line)) {
    // add() skips them too, but they must not be iterated over either
    if (!line.empty() && line[0] != '#') {
      add(line);
    }
  }
}

void glob_list::finalize() {}

bool glob_list::match_entry(const std::string& input_path, bool is_dir) const {
  if (_rules.empty()) {
    return false;
  }

#ifdef _WIN32
  std::string path = input_path;
  for (auto& c : path) {
    if (c == '\\') {
      c = '/';
    }
  }
#else
  const std::string& path = input_path;
#endif

  size_t slash = path.rfind('/');
  const char* basename = path.c_str() + (slash == std::string::npos ? 0 : slash + 1);

  // The last pattern that matches decides
  for (auto it = _rules.rbegin(); it != _rules.rend(); ++it) {
    if (it->dir_only && !is_dir) {
      continue;
    }

    if (wildmatch(it->pattern, it->basename_only ? basename : path.c_str())) {
      return !it->negated;
    }
  }

  return false;
}

bool glob_list::match(const std::string& path, bool is_dir) const {
#ifdef _WIN32
  const char* separators = "/\\";
#else
  const char* separators = "/";
#endif

  // A path below an ignored directory is ignored, whatever matches it
  for (size_t slash = path.find_first_of(separators); slash != std::string::npos;
       slash = path.find_first_of(separators, slash + 1)) {
    if (match_entry(path.substr(0, slash), true)) {
      return true;
    }
  }

  return match_entry(path, is_dir);
}

std::vector<std::string>::const_iterator glob_list::begin() const { return _inclusive_patterns.begin(); }

std::vector<std::string>::const_iterator glob_list::end() const { return _inclusive_patterns.end(); }

}  // namespace fstree
