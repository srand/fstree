#ifndef MODE_HPP
#define MODE_HPP

#include <filesystem>
#include <string>
#include <sys/stat.h>

namespace fstree {

std::string mode::str() const {
  std::string str;
  if ((_mode & S_IFLNK) == S_IFLNK) {
    str += "l";
  }
  else if ((_mode & S_IFDIR) == S_IFDIR) {
    str += "d";
  }
  else if ((_mode & S_IFREG) == S_IFREG) {
    str += "-";
  }
  else {
    str += "?";
  }
  str += (_mode & S_IRUSR) ? "r" : "-";
  str += (_mode & S_IWUSR) ? "w" : "-";
  str += (_mode & S_IXUSR) ? "x" : "-";
  str += (_mode & S_IRGRP) ? "r" : "-";
  str += (_mode & S_IWGRP) ? "w" : "-";
  str += (_mode & S_IXGRP) ? "x" : "-";
  str += (_mode & S_IROTH) ? "r" : "-";
  str += (_mode & S_IWOTH) ? "w" : "-";
  str += (_mode & S_IXOTH) ? "x" : "-";
  return str;
}

} // namespace fstree

#endif // MODE_HPP
