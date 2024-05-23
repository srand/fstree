
#include "url.hpp"

#include <gtest/gtest.h>

TEST(Url, Parse) {
  fstree::url url("http://www.example.com:8080/path/to/file.html");
  EXPECT_EQ(url.scheme(), "http");
  EXPECT_EQ(url.host(), "www.example.com:8080");
  EXPECT_EQ(url.path(), "/path/to/file.html");
}

TEST(Url, Parse_NoPort) {
  fstree::url url("http://www.example.com/path/to/file.html");
  EXPECT_EQ(url.scheme(), "http");
  EXPECT_EQ(url.host(), "www.example.com");
  EXPECT_EQ(url.path(), "/path/to/file.html");
}

TEST(Url, Parse_NoPath) {
  fstree::url url("http://www.example.com");
  EXPECT_EQ(url.scheme(), "http");
  EXPECT_EQ(url.host(), "www.example.com");
  EXPECT_EQ(url.path(), "/");
}

TEST(Url, Parse_NoPath_NoPort) {
  fstree::url url("http://www.example.com");
  EXPECT_EQ(url.scheme(), "http");
  EXPECT_EQ(url.host(), "www.example.com");
  EXPECT_EQ(url.path(), "/");
}

TEST(Url, Parse_NoPath_NoPort_NoScheme) {
  fstree::url url("www.example.com");
  EXPECT_EQ(url.scheme(), "");
  EXPECT_EQ(url.host(), "www.example.com");
  EXPECT_EQ(url.path(), "/");
}