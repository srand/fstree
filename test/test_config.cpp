
#include "argparser.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <string>
#include <vector>

namespace {

// Parses a command line given as strings
void parse(fstree::argparser& args, std::vector<std::string> argv) {
  std::vector<char*> ptrs;
  for (auto& arg : argv) {
    ptrs.push_back(arg.data());
  }
  args.parse(int(ptrs.size()), ptrs.data());
}

void set_env(const char* name, const char* value) {
#ifdef _WIN32
  _putenv_s(name, value);
#else
  setenv(name, value, 1);
#endif
}

void unset_env(const char* name) {
#ifdef _WIN32
  _putenv_s(name, "");
#else
  unsetenv(name);
#endif
}

}  // namespace

TEST(Config, ListOptionCollectsRepeatedValues) {
  fstree::argparser args;
  args.add_list_option("--pattern");
  args.add_option("--other", "default");
  parse(args, {"fstree", "--pattern", "a", "command", "--pattern", "b c", "--other", "x", "--other", "y"});

  EXPECT_EQ((std::vector<std::string>{"a", "b c"}), args.get_list_option("--pattern"));
  EXPECT_EQ("y", args.get_option("--other"));
  ASSERT_EQ(1u, args.size());
  EXPECT_EQ("command", args[0]);
}

TEST(Config, OptionWithEqualsSign) {
  fstree::argparser args;
  args.add_list_option("--pattern");
  args.add_option("--other", "default");
  parse(args, {"fstree", "--pattern=a=b", "--other=x", "--pattern=c"});

  EXPECT_EQ((std::vector<std::string>{"a=b", "c"}), args.get_list_option("--pattern"));
  EXPECT_EQ("x", args.get_option("--other"));
}

TEST(Config, ListOptionIsEmptyByDefault) {
  fstree::argparser args;
  args.add_list_option("--pattern");
  parse(args, {"fstree", "command"});

  EXPECT_TRUE(args.get_list_option("--pattern").empty());
}

TEST(Config, ListOptionFromEnvironmentLines) {
  set_env("FSTREE_TEST_PATTERN", "a\n\nb c\r\n");

  fstree::argparser args;
  args.set_env_prefix("FSTREE");
  args.add_list_option("--test-pattern");
  parse(args, {"fstree"});
  EXPECT_EQ((std::vector<std::string>{"a", "b c"}), args.get_list_option("--test-pattern"));

  // The command line replaces the environment, as for other options
  fstree::argparser args2;
  args2.set_env_prefix("FSTREE");
  args2.add_list_option("--test-pattern");
  parse(args2, {"fstree", "--test-pattern", "c", "--test-pattern", "d"});
  EXPECT_EQ((std::vector<std::string>{"c", "d"}), args2.get_list_option("--test-pattern"));

  unset_env("FSTREE_TEST_PATTERN");
}

TEST(Config, ParseSize) {
  EXPECT_EQ(fstree::parse_size("1"), 1);
  EXPECT_EQ(fstree::parse_size("1K"), 1000);
  EXPECT_EQ(fstree::parse_size("1M"), 1000 * 1000);
  EXPECT_EQ(fstree::parse_size("1G"), 1000 * 1000 * 1000);
  EXPECT_EQ(fstree::parse_size("1T"), 1000ull * 1000 * 1000 * 1000);
  EXPECT_EQ(fstree::parse_size("1Ki"), 1024);
  EXPECT_EQ(fstree::parse_size("1Mi"), 1024 * 1024);
  EXPECT_EQ(fstree::parse_size("1Gi"), 1024 * 1024 * 1024);
  EXPECT_EQ(fstree::parse_size("1Ti"), 1024ull * 1024 * 1024 * 1024);

  EXPECT_EQ(fstree::parse_size("1B"), 1);
  EXPECT_EQ(fstree::parse_size("1KB"), 1000);
  EXPECT_EQ(fstree::parse_size("1MB"), 1000 * 1000);
  EXPECT_EQ(fstree::parse_size("1GB"), 1000 * 1000 * 1000);
  EXPECT_EQ(fstree::parse_size("1TB"), 1000ull * 1000 * 1000 * 1000);
  EXPECT_EQ(fstree::parse_size("1KiB"), 1024);
  EXPECT_EQ(fstree::parse_size("1MiB"), 1024 * 1024);
  EXPECT_EQ(fstree::parse_size("1GiB"), 1024 * 1024 * 1024);
  EXPECT_EQ(fstree::parse_size("1TiB"), 1024ull * 1024 * 1024 * 1024);
}
