#pragma once

#include <string>

namespace fstree {

// How to authenticate with the services of Jolt. It is configured with the
// same environment variables as Jolt: JOLT_CACHE_TOKEN, JOLT_CA_CERT and
// JOLT_ALLOW_INSECURE_TOKEN.
class service_auth {
  std::string _token;
  std::string _ca_cert;
  bool _allow_insecure_token;

 public:
  service_auth(std::string token, std::string ca_cert, bool allow_insecure_token);

  // Returns the configuration from the environment.
  static service_auth from_environment();

  // Returns the token to send to a service, or an empty string if there is
  // none. Throws if the connection doesn't use TLS, unless sending the
  // token without it is allowed.
  const std::string& token(bool secure, const std::string& address) const;

  // Path of a PEM file with the certificates of the authorities that the
  // certificates of services are verified with, or an empty string.
  const std::string& ca_cert() const { return _ca_cert; }

  // Returns the contents of the PEM file, or an empty string if there is none.
  std::string ca_cert_pem() const;
};

}  // namespace fstree
