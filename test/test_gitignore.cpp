#include "glob_list.hpp"

#include <gtest/gtest.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// Checks glob_list against test/test_gitignore.txt, the results git itself
// reports for the same patterns and paths. See scripts/gitignore_cases.py.

namespace {

struct entry {
  std::string path;
  bool is_dir;
  bool ignored;
};

struct gitignore_case {
  std::string name;
  std::vector<std::string> patterns;
  std::vector<entry> entries;
};

// Returns the text between the first and last quote of a line, with %XX
// sequences decoded.
std::string unquote(const std::string& line) {
  size_t begin = line.find('"');
  size_t end = line.rfind('"');
  if (begin == std::string::npos || end <= begin) {
    throw std::runtime_error("malformed line: " + line);
  }

  std::string text;
  for (size_t i = begin + 1; i < end; i++) {
    if (line[i] == '%') {
      text += static_cast<char>(std::stoi(line.substr(i + 1, 2), nullptr, 16));
      i += 2;
    }
    else {
      text += line[i];
    }
  }
  return text;
}

std::vector<gitignore_case> load_cases() {
  std::ifstream file("test/test_gitignore.txt");
  if (!file) {
    throw std::runtime_error("failed to open test/test_gitignore.txt");
  }

  std::vector<gitignore_case> cases;
  std::string line;
  while (std::getline(file, line)) {
    if (line.rfind("case ", 0) == 0) {
      cases.push_back({line.substr(5), {}, {}});
    }
    else if (line.rfind("p ", 0) == 0) {
      cases.back().patterns.push_back(unquote(line));
    }
    else if (line.rfind("f ", 0) == 0 || line.rfind("d ", 0) == 0) {
      cases.back().entries.push_back({unquote(line), line[0] == 'd', line[2] == '1'});
    }
  }
  return cases;
}

std::string describe(const gitignore_case& c, const entry& e) {
  std::ostringstream text;
  text << "case: " << c.name << "\npatterns:";
  for (const auto& pattern : c.patterns) {
    text << " [" << pattern << "]";
  }
  text << "\n" << (e.is_dir ? "directory" : "file") << ": [" << e.path << "]";
  return text.str();
}

}  // namespace

TEST(Gitignore, MatchesLikeGit) {
  auto cases = load_cases();
  ASSERT_FALSE(cases.empty());

  for (const auto& c : cases) {
    fstree::glob_list ignores;
    for (const auto& pattern : c.patterns) {
      ignores.add(pattern);
    }
    ignores.finalize();

    for (const auto& e : c.entries) {
      EXPECT_EQ(e.ignored, ignores.match(e.path, e.is_dir)) << describe(c, e);
    }
  }
}

TEST(Gitignore, MatchEntryAgreesWhenParentsAreNotIgnored) {
  for (const auto& c : load_cases()) {
    fstree::glob_list ignores;
    for (const auto& pattern : c.patterns) {
      ignores.add(pattern);
    }
    ignores.finalize();

    for (const auto& e : c.entries) {
      // A walk that skips ignored directories only asks about entries whose
      // parent directories are not ignored.
      bool parent_ignored = false;
      for (size_t slash = e.path.find('/'); slash != std::string::npos; slash = e.path.find('/', slash + 1)) {
        parent_ignored = parent_ignored || ignores.match(e.path.substr(0, slash), true);
      }
      if (!parent_ignored) {
        EXPECT_EQ(e.ignored, ignores.match_entry(e.path, e.is_dir)) << describe(c, e);
      }
    }
  }
}
