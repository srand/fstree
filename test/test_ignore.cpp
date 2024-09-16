
#include "ignore.hpp"

#include <gtest/gtest.h>

TEST(Ignore, Add_Simple) {
  fstree::ignore_list ignore;
  ignore.add("*.cpp");
  ignore.add("*.h");
  // ignore.add("!*.o");
  ignore.finalize();

  EXPECT_TRUE(ignore.match("src/main.cpp"));
  EXPECT_TRUE(ignore.match("src/main.h"));
  EXPECT_FALSE(ignore.match("src/main.o"));
}

TEST(Ignore, Add_Simple_Path) {
  fstree::ignore_list ignore;
  ignore.add(".git");
  ignore.finalize();

  EXPECT_TRUE(ignore.match(".git"));
  EXPECT_TRUE(ignore.match(".git/objects"));
}

TEST(Ignore, Add_Subdir) {
  fstree::ignore_list ignore;
  ignore.add("src");
  // ignore.add("!*.o");
  ignore.finalize();

  EXPECT_TRUE(ignore.match("src/main.cpp"));
  EXPECT_TRUE(ignore.match("src/main.h"));
  // EXPECT_FALSE(ignore.match("src/main.o"));
}

TEST(Ignore, Add_Recursive) {
  fstree::ignore_list ignore;
  ignore.add("src/**");
  // ignore.add("!*.o");
  ignore.finalize();

  EXPECT_TRUE(ignore.match("src/main.cpp"));
  EXPECT_TRUE(ignore.match("src/main.h"));
  // EXPECT_FALSE(ignore.match("src/main.o"));
}

TEST(Ignore, Add_Recursive_Subdir) {
  fstree::ignore_list ignore;
  ignore.add("src/**/main.*");
  // ignore.add("!*.o");
  ignore.finalize();

  EXPECT_TRUE(ignore.match("src/main.cpp"));
  EXPECT_TRUE(ignore.match("src/main.h"));
  // EXPECT_FALSE(ignore.match("src/main.o"));
}

TEST(Ignore, Add_Recursive_Subdir_Star) {
  fstree::ignore_list ignore;
  ignore.add("src/**/main*");
  // ignore.add("!*.o");
  ignore.finalize();

  EXPECT_TRUE(ignore.match("src/main.cpp"));
  EXPECT_TRUE(ignore.match("src/main.h"));
  // EXPECT_FALSE(ignore.match("src/main.o"));
}

TEST(Ignore, Add_Recursive_Subdir_Star_Star) {
  fstree::ignore_list ignore;
  ignore.add("src/**/main**");
  // ignore.add("!*.o");
  ignore.finalize();

  EXPECT_TRUE(ignore.match("src/main.cpp"));
  EXPECT_TRUE(ignore.match("src/main.h"));
  // EXPECT_FALSE(ignore.match("src/main.o"));
}

TEST(Ignore, Add_Recursive_Subdir_Star_Star_Star) {
  fstree::ignore_list ignore;
  ignore.add("src/**/main***");
  // ignore.add("!*.o");
  ignore.finalize();

  EXPECT_TRUE(ignore.match("src/main.cpp"));
  EXPECT_TRUE(ignore.match("src/main.h"));
  // EXPECT_FALSE(ignore.match("src/main.o"));
}

TEST(Ignore, Add_Question) {
  fstree::ignore_list ignore;
  ignore.add("src/main.?pp");
  // ignore.add("!*.o");
  ignore.finalize();

  EXPECT_TRUE(ignore.match("src/main.cpp"));
  EXPECT_TRUE(ignore.match("src/main.hpp"));
  EXPECT_FALSE(ignore.match("src/main.h"));
  // EXPECT_FALSE(ignore.match("src/main.o"));
}

TEST(Ignore, Add_Star) {
  fstree::ignore_list ignore;
  ignore.add("src/main.*");
  // ignore.add("!*.o");
  ignore.finalize();

  EXPECT_TRUE(ignore.match("src/main.cpp"));
  EXPECT_TRUE(ignore.match("src/main.h"));
  // EXPECT_FALSE(ignore.match("src/main.o"));
}

TEST(Ignore, DISABLED_Add_Negation_Star) {
  fstree::ignore_list ignore;
  ignore.add("src/main.cpp");
  ignore.add("!src/main.*");
  ignore.add("!*.o");
  ignore.finalize();

  EXPECT_FALSE(ignore.match("src/main.cpp"));
  EXPECT_FALSE(ignore.match("src/main.h"));
  EXPECT_FALSE(ignore.match("src/main.o"));
}

TEST(Ignore, Load) {
  fstree::ignore_list ignore;
  ignore.load("test/test_ignore.txt");

  EXPECT_TRUE(ignore.match("src/main.cpp"));
  EXPECT_TRUE(ignore.match("src/main.h"));
  // EXPECT_FALSE(ignore.match("src/main.o"));
}
