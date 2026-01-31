#pragma once

#include <string>

namespace fstree {

class digest {
 public:
  enum class algorithm { sha1, blake3 };

  digest() = default;
  digest(const digest&) = default;
  digest(digest&&) = default;
  digest(algorithm alg, const std::string& hex) : _alg(alg), _hex(hex) {}
  virtual ~digest() = default;

  static digest parse(const std::string& str);

  const std::string& hexdigest();
  algorithm alg() const;
  std::string string() const;

 private:
  algorithm _alg;
  std::string _hex;
};

}  // namespace fstree
