#include "glob_list.hpp"

#include <gtest/gtest.h>

TEST(Glob, Add_Simple) {
  fstree::glob_list ignore;
  ignore.add("*.cpp");
  ignore.add("*.h");
  ignore.add("!*.o");
  ignore.finalize();

  EXPECT_TRUE(ignore.match("src/main.cpp"));
  EXPECT_TRUE(ignore.match("src/main.h"));
  EXPECT_FALSE(ignore.match("src/main.o"));
} 

TEST(Glob, Add_Simple_Path) {
  fstree::glob_list ignore;
  ignore.add(".git");
  ignore.finalize();

  EXPECT_TRUE(ignore.match(".git"));
  EXPECT_TRUE(ignore.match(".git/objects"));
}

TEST(Glob, Add_Subdir) {
  fstree::glob_list ignore;
  ignore.add("src");
  ignore.add("!*.o");
  ignore.finalize();

  EXPECT_TRUE(ignore.match("src/main.cpp"));
  EXPECT_TRUE(ignore.match("src/main.h"));
  EXPECT_FALSE(ignore.match("src/main.o"));
}

TEST(Glob, Add_Recursive) {
  fstree::glob_list ignore;
  ignore.add("src/**");
  ignore.add("!*.o");
  ignore.finalize();

  EXPECT_TRUE(ignore.match("src/main.cpp"));
  EXPECT_TRUE(ignore.match("src/main.h"));
  EXPECT_FALSE(ignore.match("src/main.o"));
}

TEST(Glob, Add_Recursive_Subdir) {
  fstree::glob_list ignore;
  ignore.add("src/**/main.*");
  ignore.add("!*.o");
  ignore.finalize();

  EXPECT_TRUE(ignore.match("src/main.cpp"));
  EXPECT_TRUE(ignore.match("src/main.h"));
  EXPECT_FALSE(ignore.match("src/main.o"));
}

TEST(Glob, Add_Recursive_Subdir_Star) {
  fstree::glob_list ignore;
  ignore.add("src/**/main*");
  ignore.add("!*.o");
  ignore.finalize();

  EXPECT_TRUE(ignore.match("src/main.cpp"));
  EXPECT_TRUE(ignore.match("src/main.h"));
  EXPECT_FALSE(ignore.match("src/main.o"));
}

TEST(Glob, Add_Recursive_Subdir_Star_Star) {
  fstree::glob_list ignore;
  ignore.add("src/**/main**");
  ignore.add("!*.o");
  ignore.finalize();

  EXPECT_TRUE(ignore.match("src/main.cpp"));
  EXPECT_TRUE(ignore.match("src/main.h"));
  EXPECT_FALSE(ignore.match("src/main.o"));
}

TEST(Glob, Add_Recursive_Subdir_Star_Star_Star) {
  fstree::glob_list ignore;
  ignore.add("src/**/main***");
  ignore.add("!*.o");
  ignore.finalize();

  EXPECT_TRUE(ignore.match("src/main.cpp"));
  EXPECT_TRUE(ignore.match("src/main.h"));
  EXPECT_FALSE(ignore.match("src/main.o"));
}

TEST(Glob, Add_Question) {
  fstree::glob_list ignore;
  ignore.add("src/main.?pp");
  ignore.add("!*.o");
  ignore.finalize();

  EXPECT_TRUE(ignore.match("src/main.cpp"));
  EXPECT_TRUE(ignore.match("src/main.hpp"));
  EXPECT_FALSE(ignore.match("src/main.h"));
  EXPECT_FALSE(ignore.match("src/main.o"));
}

TEST(Glob, Add_Star) {
  fstree::glob_list ignore;
  ignore.add("src/main.*");
  ignore.add("!*.o");
  ignore.finalize();

  EXPECT_TRUE(ignore.match("src/main.cpp"));
  EXPECT_TRUE(ignore.match("src/main.h"));
  EXPECT_FALSE(ignore.match("src/main.o"));
}

TEST(Glob, Add_Negation_Star) {
  fstree::glob_list ignore;
  ignore.add("src/main.cpp");
  ignore.add("!src/main.*");
  ignore.add("!*.o");
  ignore.finalize();

  EXPECT_FALSE(ignore.match("src/main.cpp"));
  EXPECT_FALSE(ignore.match("src/main.h"));
  EXPECT_FALSE(ignore.match("src/main.o"));
}

// An empty list ignores nothing at all, not even the empty path.
TEST(Glob, Empty_List_Matches_Nothing) {
  fstree::glob_list ignore;
  ignore.finalize();

  EXPECT_FALSE(ignore.match(""));
  EXPECT_FALSE(ignore.match("anything"));
  EXPECT_FALSE(ignore.match("a/b/c"));
}

// The last pattern that matches decides, as in .gitignore.
TEST(Glob, Negation_Last_Match_Wins) {
  fstree::glob_list ignore;
  ignore.add("*.log");
  ignore.add("!important.log");
  ignore.add("debug/*.log");
  ignore.finalize();

  EXPECT_TRUE(ignore.match("app.log"));
  EXPECT_FALSE(ignore.match("important.log"));
  // Re-ignored by the pattern that comes after the negation.
  EXPECT_TRUE(ignore.match("debug/important.log"));
  EXPECT_TRUE(ignore.match("debug/app.log"));
}

// A negation on its own ignores nothing; only a positive pattern can ignore.
TEST(Glob, Negation_Only) {
  fstree::glob_list ignore;
  ignore.add("!*.o");
  ignore.finalize();

  EXPECT_FALSE(ignore.match("src/main.o"));
  EXPECT_FALSE(ignore.match("src/main.cpp"));
}

// A path that no positive pattern matches is never ignored, negation or not.
TEST(Glob, Negation_Unrelated_Path) {
  fstree::glob_list ignore;
  ignore.add("*.log");
  ignore.add("!keep.txt");
  ignore.finalize();

  EXPECT_TRUE(ignore.match("a.log"));
  EXPECT_FALSE(ignore.match("keep.txt"));
  EXPECT_FALSE(ignore.match("other.txt"));
}

// Anchored patterns only match at the root, and so do anchored negations.
TEST(Glob, Negation_Anchored) {
  fstree::glob_list ignore;
  ignore.add("/build");
  ignore.add("!/build/keep.txt");
  ignore.finalize();

  EXPECT_TRUE(ignore.match("build"));
  EXPECT_TRUE(ignore.match("build/other.txt"));
  EXPECT_FALSE(ignore.match("build/keep.txt"));
  EXPECT_FALSE(ignore.match("sub/build"));
}

// Trailing slashes are stripped from negated patterns too.
TEST(Glob, Negation_Trailing_Slash) {
  fstree::glob_list ignore;
  ignore.add("build");
  ignore.add("!build/keep/");
  ignore.finalize();

  EXPECT_TRUE(ignore.match("build"));
  EXPECT_TRUE(ignore.match("build/drop"));
  EXPECT_FALSE(ignore.match("build/keep"));
}

// A negation re-includes a whole subtree, directories included.
TEST(Glob, Negation_Directory_Subtree) {
  fstree::glob_list ignore;
  ignore.add("*");
  ignore.add("!src");
  ignore.finalize();

  EXPECT_TRUE(ignore.match("build"));
  EXPECT_FALSE(ignore.match("src"));
  EXPECT_FALSE(ignore.match("src/main.cpp"));
}

// A backslash escapes a leading '!' so it can be matched literally.
TEST(Glob, Escaped_Bang) {
  fstree::glob_list ignore;
  ignore.add("\\!important");
  ignore.finalize();

  EXPECT_TRUE(ignore.match("!important"));
  EXPECT_FALSE(ignore.match("important"));
}

// A bare '!' is not a pattern and is skipped like an empty line.
TEST(Glob, Bare_Bang_Skipped) {
  fstree::glob_list ignore;
  ignore.add("*.o");
  ignore.add("!");
  ignore.finalize();

  EXPECT_TRUE(ignore.match("main.o"));
}

// Iteration exposes the positive patterns only; negations are not glob targets.
TEST(Glob, Iteration_Skips_Negations) {
  fstree::glob_list ignore;
  ignore.add("*.cpp");
  ignore.add("!*.o");
  ignore.add("*.h");
  ignore.finalize();

  std::vector<std::string> patterns(ignore.begin(), ignore.end());
  EXPECT_EQ(patterns, (std::vector<std::string>{"*.cpp", "*.h"}));
}

TEST(Glob, Load) {
  fstree::glob_list ignore;
  ignore.load("test/test_glob.txt");

  EXPECT_TRUE(ignore.match("src/main.cpp"));
  EXPECT_TRUE(ignore.match("src/main.h"));
  EXPECT_FALSE(ignore.match("src/main.o"));
}
