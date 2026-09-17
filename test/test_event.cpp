#include "event.hpp"

#include <gtest/gtest.h>

#include <string>

TEST(Event, EscapesJsonSyntaxAndControlBytes) {
  EXPECT_EQ(fstree::escape(std::string("\0\x01\b\f\n\r\t\\\"", 9)),
            "\\u0000\\u0001\\b\\f\\n\\r\\t\\\\\\\"");
}

TEST(Event, EscapesUtf8AsIndividualBytes) {
  const std::string path = "caf\xc3\xa9/\xf0\x9f\x93\x81";
  EXPECT_EQ(fstree::escape(path), "caf\\u00c3\\u00a9/\\u00f0\\u009f\\u0093\\u0081");
}

TEST(Event, EscapesInvalidUtf8Bytes) {
  const std::string path("a/\xff\xc0\xaf\xe2\x82/z", 9);
  EXPECT_EQ(fstree::escape(path), "a/\\u00ff\\u00c0\\u00af\\u00e2\\u0082/z");
}

TEST(Event, RejectsUtf8SurrogatesAndOutOfRangeCodePoints) {
  const std::string path("\xed\xa0\x80\xf4\x90\x80\x80", 7);
  EXPECT_EQ(fstree::escape(path), "\\u00ed\\u00a0\\u0080\\u00f4\\u0090\\u0080\\u0080");
}