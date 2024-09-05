
#include "argparser.hpp"

#include <gtest/gtest.h>

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
