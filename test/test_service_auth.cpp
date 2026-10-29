#include "service_auth.hpp"

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

TEST(ServiceAuth, NoToken) {
  fstree::service_auth auth("", "", false);
  EXPECT_EQ(auth.token(false, "tcp://cache"), "");
  EXPECT_EQ(auth.token(true, "tls://cache"), "");
  EXPECT_EQ(auth.ca_cert_pem(), "");
}

TEST(ServiceAuth, TokenOnlyWithTLS) {
  fstree::service_auth auth("secret", "", false);
  EXPECT_EQ(auth.token(true, "tls://cache"), "secret");
  EXPECT_THROW(auth.token(false, "tcp://cache"), std::runtime_error);
}

TEST(ServiceAuth, TokenWithoutTLSWhenAllowed) {
  fstree::service_auth auth("secret", "", true);
  EXPECT_EQ(auth.token(false, "tcp://cache"), "secret");
}

TEST(ServiceAuth, CACert) {
  auto path = std::filesystem::temp_directory_path() / "fstree_test_ca.pem";
  {
    std::ofstream file(path, std::ios::binary);
    file << "-----BEGIN CERTIFICATE-----\n";
  }
  fstree::service_auth auth("", path.string(), false);
  EXPECT_EQ(auth.ca_cert_pem(), "-----BEGIN CERTIFICATE-----\n");
  std::filesystem::remove(path);

  fstree::service_auth missing("", "/nonexistent/ca.pem", false);
  EXPECT_THROW(missing.ca_cert_pem(), std::runtime_error);
}

TEST(ServiceAuth, FromEnvironment) {
#ifdef _WIN32
  _putenv_s("JOLT_CACHE_TOKEN", "secret");
  _putenv_s("JOLT_ALLOW_INSECURE_TOKEN", "TRUE");
#else
  setenv("JOLT_CACHE_TOKEN", "secret", 1);
  setenv("JOLT_ALLOW_INSECURE_TOKEN", "TRUE", 1);
#endif
  auto auth = fstree::service_auth::from_environment();
  EXPECT_EQ(auth.token(false, "tcp://cache"), "secret");
#ifdef _WIN32
  _putenv_s("JOLT_CACHE_TOKEN", "");
  _putenv_s("JOLT_ALLOW_INSECURE_TOKEN", "");
#else
  unsetenv("JOLT_CACHE_TOKEN");
  unsetenv("JOLT_ALLOW_INSECURE_TOKEN");
#endif
}
