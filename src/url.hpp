#ifndef URL_HPP
#define URL_HPP

#include <string>

namespace fstree {

class url {
  std::string _url;

 public:
  explicit url(const std::string& url) : _url(url) {}

  std::string scheme() const {
    size_t pos = _url.find("://");
    if (pos == std::string::npos) return "";
    return _url.substr(0, pos);
  }

  std::string host() const {
    size_t pos = _url.find("://");
    if (pos == std::string::npos) return "";
    pos += 3;
    size_t end = _url.find("/", pos);
    if (end == std::string::npos) return _url.substr(pos);
    return _url.substr(pos, end - pos);
  }

  std::string path() const {
    size_t pos = _url.find("://");
    if (pos == std::string::npos) return "";
    pos += 3;
    size_t end = _url.find("/", pos);
    if (end == std::string::npos) return "";
    return _url.substr(end);
  }
};

}  // namespace fstree

#endif  // URL_HPP
