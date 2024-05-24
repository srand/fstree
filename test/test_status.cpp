
#include "status.hpp"

#include <gtest/gtest.h>

TEST(Status, FileStatus) {
  fstree::file_status status(std::filesystem::file_type::regular, std::filesystem::perms::owner_read);
  EXPECT_EQ(status.type(), std::filesystem::file_type::regular);
  EXPECT_EQ(status.permissions(), std::filesystem::perms::owner_read);
}

TEST(Status, FileStatusCast) {
  fstree::file_status status(std::filesystem::file_type::regular, std::filesystem::perms::owner_read);
  uint32_t m = status;
  EXPECT_EQ(m, 0x01000100);
}

TEST(Status, FileStatusFromInt) {
  fstree::file_status status(0x01000100);
  EXPECT_EQ(status.type(), std::filesystem::file_type::regular);
  EXPECT_EQ(status.permissions(), std::filesystem::perms::owner_read);
}

TEST(Status, FileStatusFromIntDirectory) {
  fstree::file_status status(0x02000100);
  EXPECT_EQ(status.type(), std::filesystem::file_type::directory);
  EXPECT_EQ(status.permissions(), std::filesystem::perms::owner_read);
}

TEST(Status, FileStatusFromIntSymlink) {
  fstree::file_status status(0x04000100);
  EXPECT_EQ(status.type(), std::filesystem::file_type::symlink);
  EXPECT_EQ(status.permissions(), std::filesystem::perms::owner_read);
}

TEST(Status, FileStatusFromIntNone) {
  fstree::file_status status(0x00000000);
  EXPECT_EQ(status.type(), std::filesystem::file_type::none);
  EXPECT_EQ(status.permissions(), std::filesystem::perms::none);
}

TEST(Status, FileStatusFromIntInvalid) {
  fstree::file_status status(0x80000000);
  EXPECT_EQ(status.type(), std::filesystem::file_type::none);
  EXPECT_EQ(status.permissions(), std::filesystem::perms::none);
}
