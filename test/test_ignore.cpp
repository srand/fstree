
#include "ignore.hpp"

#include <gtest/gtest.h>

TEST(Ignore, Add_Simple) {
  fstree::ignore_list ignore;
  ignore.add("*.cpp");
  ignore.add("*.h");
  ignore.add("!*.o");
  ignore.finalize();

  EXPECT_EQ(ignore.match("src/main.cpp"), true);
  EXPECT_EQ(ignore.match("src/main.h"), true);
  EXPECT_EQ(ignore.match("src/main.o"), false);
}

TEST(Ignore, Add_Subdir) {
  fstree::ignore_list ignore;
  ignore.add("src");
  ignore.add("!*.o");
  ignore.finalize();

  EXPECT_EQ(ignore.match("src/main.cpp"), true);
  EXPECT_EQ(ignore.match("src/main.h"), true);
  EXPECT_EQ(ignore.match("src/main.o"), false);
}

TEST(Ignore, Add_Recursive) {
  fstree::ignore_list ignore;
  ignore.add("src/**");
  ignore.add("!*.o");
  ignore.finalize();

  EXPECT_EQ(ignore.match("src/main.cpp"), true);
  EXPECT_EQ(ignore.match("src/main.h"), true);
  EXPECT_EQ(ignore.match("src/main.o"), false);
}

TEST(Ignore, Add_Recursive_Subdir) {
  fstree::ignore_list ignore;
  ignore.add("src/**/main.*");
  ignore.add("!*.o");
  ignore.finalize();

  EXPECT_EQ(ignore.match("src/main.cpp"), true);
  EXPECT_EQ(ignore.match("src/main.h"), true);
  EXPECT_EQ(ignore.match("src/main.o"), false);
}

TEST(Ignore, Add_Recursive_Subdir_Star) {
  fstree::ignore_list ignore;
  ignore.add("src/**/main*");
  ignore.add("!*.o");
  ignore.finalize();

  EXPECT_EQ(ignore.match("src/main.cpp"), true);
  EXPECT_EQ(ignore.match("src/main.h"), true);
  EXPECT_EQ(ignore.match("src/main.o"), false);
}

TEST(Ignore, Add_Recursive_Subdir_Star_Star) {
  fstree::ignore_list ignore;
  ignore.add("src/**/main**");
  ignore.add("!*.o");
  ignore.finalize();

  EXPECT_EQ(ignore.match("src/main.cpp"), true);
  EXPECT_EQ(ignore.match("src/main.h"), true);
  EXPECT_EQ(ignore.match("src/main.o"), false);
}

TEST(Ignore, Add_Recursive_Subdir_Star_Star_Star) {
  fstree::ignore_list ignore;
  ignore.add("src/**/main***");
  ignore.add("!*.o");
  ignore.finalize();

  EXPECT_EQ(ignore.match("src/main.cpp"), true);
  EXPECT_EQ(ignore.match("src/main.h"), true);
  EXPECT_EQ(ignore.match("src/main.o"), false);
}

TEST(Ignore, Add_Question) {
  fstree::ignore_list ignore;
  ignore.add("src/main.?pp");
  ignore.add("!*.o");
  ignore.finalize();

  EXPECT_EQ(ignore.match("src/main.cpp"), true);
  EXPECT_EQ(ignore.match("src/main.hpp"), true);
  EXPECT_EQ(ignore.match("src/main.h"), false);
  EXPECT_EQ(ignore.match("src/main.o"), false);
}

TEST(Ignore, Add_Star) {
  fstree::ignore_list ignore;
  ignore.add("src/main.*");
  ignore.add("!*.o");
  ignore.finalize();

  EXPECT_EQ(ignore.match("src/main.cpp"), true);
  EXPECT_EQ(ignore.match("src/main.h"), true);
  EXPECT_EQ(ignore.match("src/main.o"), false);
}

TEST(Ignore, Add_Negation_Star) {
  fstree::ignore_list ignore;
  ignore.add("src/main.cpp");
  ignore.add("!src/main.*");
  ignore.add("!*.o");
  ignore.finalize();

  EXPECT_EQ(ignore.match("src/main.cpp"), false);
  EXPECT_EQ(ignore.match("src/main.h"), false);
  EXPECT_EQ(ignore.match("src/main.o"), false);
}

TEST(Ignore, Load) {
  fstree::ignore_list ignore;
  ignore.load("test/test_ignore.txt");

  EXPECT_EQ(ignore.match("src/main.cpp"), true);
  EXPECT_EQ(ignore.match("src/main.h"), true);
  EXPECT_EQ(ignore.match("src/main.o"), false);
}
