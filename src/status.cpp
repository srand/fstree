#include "status.hpp"

#include <filesystem>
#include <string>
#include <sys/stat.h>

namespace fstree {

// Constructor implementations
file_status::file_status(uint32_t m) {
  std::filesystem::perms perms = (std::filesystem::perms)(m & (uint32_t)std::filesystem::perms::all);
  std::filesystem::file_type type;

  switch (m & internal_type::mask) {
    case internal_type::regular:
      type = std::filesystem::file_type::regular;
      break;
    case internal_type::directory:
      type = std::filesystem::file_type::directory;
      break;
    case internal_type::symlink:
      type = std::filesystem::file_type::symlink;
      break;
    default:
      type = std::filesystem::file_type::none;
      break;
  }

  *this = file_status(type, perms);
}

file_status::file_status(std::filesystem::file_type type, std::filesystem::perms perms)
    : std::filesystem::file_status(type, perms) {}

file_status::file_status() {}

// Cast to unsigned int
file_status::operator uint32_t() const {
  uint32_t m = (uint32_t)(permissions() & std::filesystem::perms::mask);

  switch (type()) {
    case std::filesystem::file_type::directory:
      m |= internal_type::directory;
      break;
    case std::filesystem::file_type::symlink:
      m |= internal_type::symlink;
      break;
    case std::filesystem::file_type::regular:
      m |= internal_type::regular;
      break;
    default:
      break;
  }

  return m;
}

// Cast to bool
file_status::operator bool() const { 
  return type() != std::filesystem::file_type::none; 
}

std::string file_status::str() const {
  std::string str;

  switch (type()) {
    case std::filesystem::file_type::directory:
      str += "d";
      break;
    case std::filesystem::file_type::symlink:
      str += "l";
      break;
    default:
      str += "-";
      break;
  }

  auto perms = permissions();

  str += ((perms & std::filesystem::perms::owner_read) != std::filesystem::perms::none) ? "r" : "-";
  str += ((perms & std::filesystem::perms::owner_write) != std::filesystem::perms::none) ? "w" : "-";
  str += ((perms & std::filesystem::perms::owner_exec) != std::filesystem::perms::none) ? "x" : "-";
  str += ((perms & std::filesystem::perms::group_read) != std::filesystem::perms::none) ? "r" : "-";
  str += ((perms & std::filesystem::perms::group_write) != std::filesystem::perms::none) ? "w" : "-";
  str += ((perms & std::filesystem::perms::group_exec) != std::filesystem::perms::none) ? "x" : "-";
  str += ((perms & std::filesystem::perms::others_read) != std::filesystem::perms::none) ? "r" : "-";
  str += ((perms & std::filesystem::perms::others_write) != std::filesystem::perms::none) ? "w" : "-";
  str += ((perms & std::filesystem::perms::others_exec) != std::filesystem::perms::none) ? "x" : "-";
  return str;
}

bool file_status::is_regular() const { 
  return type() == std::filesystem::file_type::regular; 
}

bool file_status::is_directory() const { 
  return type() == std::filesystem::file_type::directory; 
}

bool file_status::is_symlink() const { 
  return type() == std::filesystem::file_type::symlink; 
}

}  // namespace fstree