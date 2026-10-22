#include "cache.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;
using namespace std::chrono_literals;

class EvictionClaimTest : public ::testing::Test {
 protected:
  void SetUp() override {
    dir = fs::temp_directory_path() / "fstree_test_evict";
    fs::remove_all(dir);
  }

  void TearDown() override { fs::remove_all(dir); }

  // Moves the time eviction was last claimed by the given amount.
  void ShiftStamp(fs::file_time_type::duration by) {
    fs::path stamp = dir / "evict.stamp";
    fs::last_write_time(stamp, fs::last_write_time(stamp) + by);
  }

  fs::path dir;
};

TEST_F(EvictionClaimTest, ClaimsOncePerInterval) {
  fstree::cache cache(dir, fstree::cache::default_max_size, fstree::cache::default_retention);

  // Eviction has never run, so it is due, and then not until the interval
  // has passed.
  EXPECT_TRUE(cache.claim_eviction(60s));
  EXPECT_FALSE(cache.claim_eviction(60s));

  ShiftStamp(-30s);
  EXPECT_FALSE(cache.claim_eviction(60s));

  ShiftStamp(-60s);
  EXPECT_TRUE(cache.claim_eviction(60s));
  EXPECT_FALSE(cache.claim_eviction(60s));
}

TEST_F(EvictionClaimTest, ClaimsWhenLastClaimIsInTheFuture) {
  fstree::cache cache(dir, fstree::cache::default_max_size, fstree::cache::default_retention);

  // A clock that was set back must not keep eviction from running.
  EXPECT_TRUE(cache.claim_eviction(60s));
  ShiftStamp(1h);
  EXPECT_TRUE(cache.claim_eviction(60s));
}

TEST_F(EvictionClaimTest, ClaimsAlwaysWithoutInterval) {
  fstree::cache cache(dir, fstree::cache::default_max_size, fstree::cache::default_retention);

  EXPECT_TRUE(cache.claim_eviction(0s));
  EXPECT_TRUE(cache.claim_eviction(0s));
}
