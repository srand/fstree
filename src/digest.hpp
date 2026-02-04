#pragma once

#include <ostream>
#include <string>

namespace fstree {

class digest {
public:
  // Parses a digest from a string representation.
  static digest parse(const std::string& str);

 public:
  enum class algorithm { none, sha1, blake3 };

  digest() : _alg(algorithm::none), _hex("") {}
  digest(const digest& other) : _alg(other._alg), _hex(other._hex) {}
  digest(digest&& other) : _alg(other._alg), _hex(std::move(other._hex)) {}
  digest(algorithm alg, const std::string& hex) : _alg(alg), _hex(hex) {}
  virtual ~digest() = default;

  digest& operator=(const digest&) = default;
  bool operator==(const digest& other) const { return _alg == other._alg && _hex == other._hex; }

  // Returns true if the digest is empty.
  bool empty() const;

  // Returns the hex representation of the digest.
  const std::string& hexdigest() const;

  // Returns the algorithm used for the digest.
  algorithm alg() const;

  // Returns the string representation of the digest, e.g., "sha1:abcd1234..."
  std::string string() const;

 private:
  algorithm _alg;
  std::string _hex;
};

std::ostream& operator<<(std::ostream& os, const digest& digest);

}  // namespace fstree
