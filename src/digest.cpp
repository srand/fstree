#include "digest.hpp"

#include <stdexcept>

namespace fstree {

digest::algorithm digest::alg() const { return _alg; }
const std::string& digest::hexdigest() { return _hex; }
std::string digest::string() const {
  switch (_alg) {
    case algorithm::sha1:
      return "sha1:" + _hex;
    case algorithm::blake3:
      return "blake3:" + _hex;
  }
  return "";
}

digest digest::parse(const std::string& str) {
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
    return digest(algorithm::sha1, hex);
  } else if (alg_str == "blake3") {
    return digest(algorithm::blake3, hex);
  } else {
    throw std::invalid_argument("unknown algorithm: " + alg_str);
  }
}

}  // namespace fstree
