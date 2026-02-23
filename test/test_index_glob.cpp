
#include "index.hpp"
#include "inode.hpp"
#include "glob_list.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

using namespace fstree;
namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------

class IndexGlobTest : public ::testing::Test {
protected:
  void SetUp() override {
    test_dir = fs::temp_directory_path() / "fstree_test_index_glob";
    fs::remove_all(test_dir);
    fs::create_directories(test_dir);
  }

  void TearDown() override {
    fs::remove_all(test_dir);
  }

  void CreateFile(const std::string& rel) {
    fs::path full = test_dir / rel;
    fs::create_directories(full.parent_path());
    std::ofstream(full) << "x";
  }

  // Build an index over test_dir and return it (tree-populated via refresh).
  fstree::index BuildIndex() {
    fstree::glob_list ignores;
    fstree::index idx(test_dir, ignores);
    idx.refresh();
    return idx;
  }

  // Collect the paths of all matched inodes as a sorted set of strings.
  static std::set<std::string> Paths(const std::vector<fstree::inode::ptr>& inodes) {
    std::set<std::string> result;
    for (const auto& n : inodes) {
      result.insert(NormalizePath(n->path()));
    }
    return result;
  }

  static std::string NormalizePath(std::string p) {
    for (auto& c : p)
      if (c == '\\') c = '/';
    return p;
  }

  fs::path test_dir;
};

// ---------------------------------------------------------------------------
// Single-level patterns  (no '/' in pattern → un-anchored, implicit **)
// ---------------------------------------------------------------------------

TEST_F(IndexGlobTest, StarExtension_MatchesAtAnyDepth) {
  CreateFile("README.md");
  CreateFile("src/main.cpp");
  CreateFile("src/lib.cpp");
  CreateFile("src/helper.h");
  CreateFile("src/utils/util.cpp");
  CreateFile("include/foo.h");

  auto idx = BuildIndex();
  auto result = Paths(idx.glob("*.cpp"));

  EXPECT_EQ(result.count("src/main.cpp"),      1);
  EXPECT_EQ(result.count("src/lib.cpp"),       1);
  EXPECT_EQ(result.count("src/utils/util.cpp"),1);
  EXPECT_EQ(result.count("README.md"),         0);
  EXPECT_EQ(result.count("src/helper.h"),      0);
}

TEST_F(IndexGlobTest, StarExtension_NoMatch) {
  CreateFile("src/main.cpp");

  auto idx = BuildIndex();
  EXPECT_TRUE(idx.glob("*.xyz").empty());
}

TEST_F(IndexGlobTest, QuestionMark_SingleChar) {
  CreateFile("src/main.cpp");
  CreateFile("src/main.hpp");
  CreateFile("src/main.h");

  auto idx = BuildIndex();
  auto result = Paths(idx.glob("main.?pp"));

  EXPECT_EQ(result.count("src/main.cpp"), 1);
  EXPECT_EQ(result.count("src/main.hpp"), 1);
  // "main.h" has only one char after '.', not two → no match
  EXPECT_EQ(result.count("src/main.h"),   0);
}

TEST_F(IndexGlobTest, LiteralFilename_UnAnchored) {
  CreateFile("README.md");
  CreateFile("docs/README.md");

  auto idx = BuildIndex();
  auto result = Paths(idx.glob("README.md"));

  EXPECT_EQ(result.count("README.md"),      1);
  EXPECT_EQ(result.count("docs/README.md"), 1);
}

// ---------------------------------------------------------------------------
// Patterns with a directory component (anchored by the leading dir literal)
// ---------------------------------------------------------------------------

TEST_F(IndexGlobTest, DirSlashStar_OnlyDirectChildren) {
  CreateFile("src/main.cpp");
  CreateFile("src/lib.cpp");
  CreateFile("src/helper.h");
  CreateFile("src/utils/util.cpp");   // sub-dir – must NOT match src/*.cpp
  CreateFile("other/foo.cpp");        // different directory

  auto idx = BuildIndex();
  auto result = Paths(idx.glob("src/*.cpp"));

  EXPECT_EQ(result.count("src/main.cpp"),       1);
  EXPECT_EQ(result.count("src/lib.cpp"),        1);
  EXPECT_EQ(result.count("src/utils/util.cpp"), 0);  // one level too deep
  EXPECT_EQ(result.count("other/foo.cpp"),      0);  // wrong dir
  EXPECT_EQ(result.count("src/helper.h"),       0);  // wrong extension
}

TEST_F(IndexGlobTest, DirSlashQuestion_SingleChar) {
  CreateFile("src/main.cpp");
  CreateFile("src/main.hpp");

  auto idx = BuildIndex();
  auto result = Paths(idx.glob("src/main.?pp"));

  EXPECT_EQ(result.count("src/main.cpp"), 1);
  EXPECT_EQ(result.count("src/main.hpp"), 1);
}

TEST_F(IndexGlobTest, DirSlashLiteral_ExactMatch) {
  CreateFile("src/main.cpp");
  CreateFile("lib/main.cpp");

  auto idx = BuildIndex();
  auto result = Paths(idx.glob("src/main.cpp"));

  EXPECT_EQ(result.count("src/main.cpp"), 1);
  EXPECT_EQ(result.count("lib/main.cpp"), 0);
}

// ---------------------------------------------------------------------------
// Recursive ** patterns
// ---------------------------------------------------------------------------

TEST_F(IndexGlobTest, DoubleStar_AllFiles) {
  CreateFile("a.txt");
  CreateFile("src/b.txt");
  CreateFile("src/utils/c.txt");

  auto idx = BuildIndex();
  auto result = Paths(idx.glob("**"));

  // '**' should match every inode (files and directories)
  EXPECT_GE(result.size(), 3u);
  EXPECT_EQ(result.count("a.txt"),            1);
  EXPECT_EQ(result.count("src/b.txt"),        1);
  EXPECT_EQ(result.count("src/utils/c.txt"),  1);
}

TEST_F(IndexGlobTest, DoubleStar_PrefixDir) {
  CreateFile("src/main.cpp");
  CreateFile("src/lib.cpp");
  CreateFile("src/utils/util.cpp");
  CreateFile("other/foo.cpp");

  auto idx = BuildIndex();
  auto result = Paths(idx.glob("src/**"));

  EXPECT_EQ(result.count("src/main.cpp"),       1);
  EXPECT_EQ(result.count("src/lib.cpp"),        1);
  EXPECT_EQ(result.count("src/utils/util.cpp"), 1);
  EXPECT_EQ(result.count("other/foo.cpp"),      0);
}

TEST_F(IndexGlobTest, DoubleStar_PrefixDir_WithExtension) {
  CreateFile("src/main.cpp");
  CreateFile("src/lib.cpp");
  CreateFile("src/helper.h");
  CreateFile("src/utils/util.cpp");
  CreateFile("src/utils/types.h");
  CreateFile("other/foo.cpp");

  auto idx = BuildIndex();
  auto result = Paths(idx.glob("src/**/*.cpp"));

  EXPECT_EQ(result.count("src/main.cpp"),       1);
  EXPECT_EQ(result.count("src/lib.cpp"),        1);
  EXPECT_EQ(result.count("src/utils/util.cpp"), 1);
  EXPECT_EQ(result.count("src/helper.h"),       0);
  EXPECT_EQ(result.count("src/utils/types.h"),  0);
  EXPECT_EQ(result.count("other/foo.cpp"),      0);
}

TEST_F(IndexGlobTest, DoubleStar_InMiddle) {
  CreateFile("src/main.cpp");
  CreateFile("src/a/b/main.cpp");
  CreateFile("src/a/b/c/main.cpp");
  CreateFile("include/main.cpp");

  auto idx = BuildIndex();
  auto result = Paths(idx.glob("src/**/main.cpp"));

  EXPECT_EQ(result.count("src/main.cpp"),         1);
  EXPECT_EQ(result.count("src/a/b/main.cpp"),     1);
  EXPECT_EQ(result.count("src/a/b/c/main.cpp"),   1);
  EXPECT_EQ(result.count("include/main.cpp"),     0);
}

TEST_F(IndexGlobTest, DoubleStar_UnAnchored_MatchAtAnyDepth) {
  CreateFile("src/main.cpp");
  CreateFile("src/utils/util.cpp");
  CreateFile("include/impl/foo.cpp");

  auto idx = BuildIndex();
  auto result = Paths(idx.glob("**/*.cpp"));

  EXPECT_EQ(result.count("src/main.cpp"),            1);
  EXPECT_EQ(result.count("src/utils/util.cpp"),      1);
  EXPECT_EQ(result.count("include/impl/foo.cpp"),    1);
}

// ---------------------------------------------------------------------------
// Anchored patterns (leading '/')
// ---------------------------------------------------------------------------

TEST_F(IndexGlobTest, AnchoredSlash_RootOnly) {
  CreateFile("README.md");
  CreateFile("docs/README.md");

  auto idx = BuildIndex();
  auto result = Paths(idx.glob("/README.md"));

  EXPECT_EQ(result.count("README.md"),       1);
  EXPECT_EQ(result.count("docs/README.md"),  0);
}

TEST_F(IndexGlobTest, AnchoredSlash_DirChildren) {
  CreateFile("src/main.cpp");
  CreateFile("src/lib.cpp");
  CreateFile("src/utils/util.cpp");

  auto idx = BuildIndex();
  auto result = Paths(idx.glob("/src/*.cpp"));

  EXPECT_EQ(result.count("src/main.cpp"),       1);
  EXPECT_EQ(result.count("src/lib.cpp"),        1);
  EXPECT_EQ(result.count("src/utils/util.cpp"), 0);  // sub-dir, anchored
}

// ---------------------------------------------------------------------------
// glob(const glob_list&) overload – multiple patterns, deduplication
// ---------------------------------------------------------------------------

TEST_F(IndexGlobTest, GlobList_MultiplePatterns) {
  CreateFile("src/main.cpp");
  CreateFile("src/main.hpp");
  CreateFile("README.md");

  glob_list patterns;
  patterns.add("*.cpp");
  patterns.add("*.hpp");
  patterns.finalize();

  auto idx = BuildIndex();
  auto result = Paths(idx.glob(patterns));

  EXPECT_EQ(result.count("src/main.cpp"), 1);
  EXPECT_EQ(result.count("src/main.hpp"), 1);
  EXPECT_EQ(result.count("README.md"),    0);
}

TEST_F(IndexGlobTest, GlobList_Deduplication) {
  CreateFile("src/main.cpp");

  // Both patterns match the same file
  glob_list patterns;
  patterns.add("*.cpp");
  patterns.add("src/main.cpp");
  patterns.finalize();

  auto idx = BuildIndex();
  auto result = idx.glob(patterns);

  // Exactly one entry, not two
  long count = std::count_if(result.begin(), result.end(), [](const inode::ptr& n) {
    return n->path() == "src/main.cpp";
  });
  EXPECT_EQ(count, 1);
}

// ---------------------------------------------------------------------------
// Edge cases
// ---------------------------------------------------------------------------

TEST_F(IndexGlobTest, EmptyIndex_ReturnsEmpty) {
  auto idx = BuildIndex();  // empty directory
  EXPECT_TRUE(idx.glob("*.cpp").empty());
  EXPECT_TRUE(idx.glob("**").empty());
}

TEST_F(IndexGlobTest, StarStar_MatchesFileAtRoot) {
  CreateFile("top.txt");
  CreateFile("sub/deep.txt");

  auto idx = BuildIndex();
  auto result = Paths(idx.glob("**"));

  EXPECT_EQ(result.count("top.txt"),    1);
  EXPECT_EQ(result.count("sub/deep.txt"), 1);
}

TEST_F(IndexGlobTest, StarOnly_MatchesRootLevel) {
  CreateFile("top.txt");
  CreateFile("top.cpp");
  CreateFile("sub/deep.txt");

  auto idx = BuildIndex();
  // '*' alone should match any single-component name at any depth
  // (un-anchored ⟹ ["**","*"])
  auto result = Paths(idx.glob("*"));

  EXPECT_EQ(result.count("top.txt"),    1);
  EXPECT_EQ(result.count("top.cpp"),    1);
  // sub is a directory – also matched by '*'
  EXPECT_EQ(result.count("sub/deep.txt"), 1);
}

TEST_F(IndexGlobTest, ThreeLevelPattern) {
  CreateFile("a/b/c.txt");
  CreateFile("a/b/d.txt");
  CreateFile("a/x/c.txt");

  auto idx = BuildIndex();
  auto result = Paths(idx.glob("a/b/*.txt"));

  EXPECT_EQ(result.count("a/b/c.txt"), 1);
  EXPECT_EQ(result.count("a/b/d.txt"), 1);
  EXPECT_EQ(result.count("a/x/c.txt"), 0);
}

TEST_F(IndexGlobTest, WildcardInDir_MatchesDirComponent) {
  CreateFile("src123/main.cpp");
  CreateFile("src456/lib.cpp");
  CreateFile("other/foo.cpp");

  auto idx = BuildIndex();
  auto result = Paths(idx.glob("src*/*.cpp"));

  EXPECT_EQ(result.count("src123/main.cpp"), 1);
  EXPECT_EQ(result.count("src456/lib.cpp"),  1);
  EXPECT_EQ(result.count("other/foo.cpp"),   0);
}
