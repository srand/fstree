#ifdef FSTREE_ENABLE_HTTP_REMOTE

#include "remote_http.hpp"
#include "url.hpp"

#include <gtest/gtest.h>

TEST(RemoteHttp, ObjectUrlIsShardedByHexDigest) {
  fstree::remote_http remote(fstree::url("http://localhost:8080/cache"));

  // The algorithm is not part of the path, digests of different algorithms
  // have hex strings of different length and cannot collide.
  EXPECT_EQ(
      "http://localhost:8080/cache/fe/3d8d/834ae52bf4961af02f1b2db179331137c3281d1dd8a2dd565949eacabe",
      remote.url_path(fstree::digest::parse(
          "blake3:fe3d8d834ae52bf4961af02f1b2db179331137c3281d1dd8a2dd565949eacabe")));
  EXPECT_EQ(
      "http://localhost:8080/cache/7d/97e9/8f8af710c7e7fe703abc8f639e0ee507c4",
      remote.url_path(fstree::digest::parse("sha1:7d97e98f8af710c7e7fe703abc8f639e0ee507c4")));
}

#endif  // FSTREE_ENABLE_HTTP_REMOTE
