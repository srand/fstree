
#include "glob_list.hpp"

#include <gtest/gtest.h>

TEST(Glob, Add_Simple) {
  fstree::glob_list ignore;
  ignore.add("*.cpp");
  ignore.add("*.h");
  // ignore.add("!*.o");
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
  // ignore.add("!*.o");
  ignore.finalize();

  EXPECT_TRUE(ignore.match("src/main.cpp"));
  EXPECT_TRUE(ignore.match("src/main.h"));
  // EXPECT_FALSE(ignore.match("src/main.o"));
}

TEST(Glob, Add_Recursive) {
  fstree::glob_list ignore;
  ignore.add("src/**");
  // ignore.add("!*.o");
  ignore.finalize();

  EXPECT_TRUE(ignore.match("src/main.cpp"));
  EXPECT_TRUE(ignore.match("src/main.h"));
  // EXPECT_FALSE(ignore.match("src/main.o"));
}

TEST(Glob, Add_Recursive_Subdir) {
  fstree::glob_list ignore;
  ignore.add("src/**/main.*");
  // ignore.add("!*.o");
  ignore.finalize();

  EXPECT_TRUE(ignore.match("src/main.cpp"));
  EXPECT_TRUE(ignore.match("src/main.h"));
  // EXPECT_FALSE(ignore.match("src/main.o"));
}

TEST(Glob, Add_Recursive_Subdir_Star) {
  fstree::glob_list ignore;
  ignore.add("src/**/main*");
  // ignore.add("!*.o");
  ignore.finalize();

  EXPECT_TRUE(ignore.match("src/main.cpp"));
  EXPECT_TRUE(ignore.match("src/main.h"));
  // EXPECT_FALSE(ignore.match("src/main.o"));
}

TEST(Glob, Add_Recursive_Subdir_Star_Star) {
  fstree::glob_list ignore;
  ignore.add("src/**/main**");
  // ignore.add("!*.o");
  ignore.finalize();

  EXPECT_TRUE(ignore.match("src/main.cpp"));
  EXPECT_TRUE(ignore.match("src/main.h"));
  // EXPECT_FALSE(ignore.match("src/main.o"));
}

TEST(Glob, Add_Recursive_Subdir_Star_Star_Star) {
  fstree::glob_list ignore;
  ignore.add("src/**/main***");
  // ignore.add("!*.o");
  ignore.finalize();

  EXPECT_TRUE(ignore.match("src/main.cpp"));
  EXPECT_TRUE(ignore.match("src/main.h"));
  // EXPECT_FALSE(ignore.match("src/main.o"));
}

TEST(Glob, Add_Question) {
  fstree::glob_list ignore;
  ignore.add("src/main.?pp");
  // ignore.add("!*.o");
  ignore.finalize();

  EXPECT_TRUE(ignore.match("src/main.cpp"));
  EXPECT_TRUE(ignore.match("src/main.hpp"));
  EXPECT_FALSE(ignore.match("src/main.h"));
  // EXPECT_FALSE(ignore.match("src/main.o"));
}

TEST(Glob, Add_Star) {
  fstree::glob_list ignore;
  ignore.add("src/main.*");
  // ignore.add("!*.o");
  ignore.finalize();

  EXPECT_TRUE(ignore.match("src/main.cpp"));
  EXPECT_TRUE(ignore.match("src/main.h"));
  // EXPECT_FALSE(ignore.match("src/main.o"));
}

TEST(Glob, DISABLED_Add_Negation_Star) {
  fstree::glob_list ignore;
  ignore.add("src/main.cpp");
  ignore.add("!src/main.*");
  ignore.add("!*.o");
  ignore.finalize();

  EXPECT_FALSE(ignore.match("src/main.cpp"));
  EXPECT_FALSE(ignore.match("src/main.h"));
  EXPECT_FALSE(ignore.match("src/main.o"));
}

TEST(Glob, Load) {
  fstree::glob_list ignore;
  ignore.load("test/test_glob.txt");

  EXPECT_TRUE(ignore.match("src/main.cpp"));
  EXPECT_TRUE(ignore.match("src/main.h"));
  // EXPECT_FALSE(ignore.match("src/main.o"));
}
