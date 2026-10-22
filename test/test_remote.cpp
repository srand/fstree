#include "hash.hpp"
#include "remote.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

class VerifyDownloadTest : public ::testing::Test {
 protected:
  void SetUp() override {
    dir = fs::temp_directory_path() / "fstree_test_verify_download";
    fs::remove_all(dir);
    fs::create_directories(dir);
    file = dir / "download";
    std::ofstream(file, std::ios::binary) << "content";
  }

  void TearDown() override { fs::remove_all(dir); }

  fs::path dir, file;
};

TEST_F(VerifyDownloadTest, AcceptsMatchingContent) {
  EXPECT_NO_THROW(fstree::verify_download(fstree::hashsum_hex_file(file), file));
  EXPECT_TRUE(fs::exists(file));
}

TEST_F(VerifyDownloadTest, RejectsAndRemovesOtherContent) {
  fs::path other = dir / "other";
  std::ofstream(other, std::ios::binary) << "other content";

  EXPECT_THROW(fstree::verify_download(fstree::hashsum_hex_file(other), file), std::runtime_error);
  EXPECT_FALSE(fs::exists(file));
}
