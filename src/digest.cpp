#include "digest.hpp"

#include <stdexcept>

namespace fstree {

digest::algorithm digest::alg() const { return _alg; }

bool digest::empty() const { return _hex.empty(); }

const std::string& digest::hexdigest() const { return _hex; }

std::string digest::string() const {
  switch (_alg) {
    case algorithm::none:
      return "";
    case algorithm::sha1:
      return "sha1:" + _hex;
    case algorithm::blake3:
      return "blake3:" + _hex;
  }
  return "";
}

digest digest::parse(const std::string& str) {
  if (str.empty()) {
    return digest();
  }

  auto pos = str.find(':');
  if (pos == std::string::npos) {
    if (str.length() == 40) {
      return digest(algorithm::sha1, str);
    } else if (str.length() == 64) {
      return digest(algorithm::blake3, str);
    } else {
      throw std::invalid_argument("cannot determine algorithm for digest: " + str);
    }
  }

  std::string alg_str = str.substr(0, pos);
  std::string hex = str.substr(pos + 1);
  if (alg_str == "sha1") {
    if (hex.length() != 40) {
      throw std::invalid_argument("invalid sha1 digest length: " + hex);
    }
    return digest(algorithm::sha1, hex);
  } else if (alg_str == "blake3") {
    if (hex.length() != 64) {
      throw std::invalid_argument("invalid blake3 digest length: " + hex);
    }
    return digest(algorithm::blake3, hex);
  } else {
    throw std::invalid_argument("unknown algorithm: " + alg_str);
  }
}

std::ostream& operator<<(std::ostream& os, const digest& digest){
  os << digest.string();
  return os;
}

}  // namespace fstree
