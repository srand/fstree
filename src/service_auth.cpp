#include "service_auth.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace fstree {

namespace {

std::string getenv_string(const char* name) {
  const char* value = std::getenv(name);
  return value ? value : "";
}

bool is_true(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return std::tolower(c); });
  return value == "1" || value == "true" || value == "yes" || value == "on";
}

}  // namespace

service_auth::service_auth(std::string token, std::string ca_cert, bool allow_insecure_token)
    : _token(std::move(token)), _ca_cert(std::move(ca_cert)), _allow_insecure_token(allow_insecure_token) {}

service_auth service_auth::from_environment() {
  return service_auth(
      getenv_string("JOLT_CACHE_TOKEN"),
      getenv_string("JOLT_CA_CERT"),
      is_true(getenv_string("JOLT_ALLOW_INSECURE_TOKEN")));
}

const std::string& service_auth::token(bool secure, const std::string& address) const {
  if (!_token.empty() && !secure && !_allow_insecure_token) {
    throw std::runtime_error("refusing to send the cache token without TLS to " + address);
  }
  return _token;
}

std::string service_auth::ca_cert_pem() const {
  if (_ca_cert.empty()) {
    return "";
  }
  std::ifstream file(_ca_cert, std::ios::binary);
  if (!file) {
    throw std::runtime_error("failed to read CA certificates: " + _ca_cert);
  }
  std::stringstream contents;
  contents << file.rdbuf();
  return contents.str();
}

}  // namespace fstree
