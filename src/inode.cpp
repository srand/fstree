#include "inode.hpp"

#include <cstring>

namespace fstree {

static const uint16_t g_magic = 0x3eee;
static const uint16_t g_version = 1;

std::ostream& operator<<(std::ostream& os, const inode& inode) {
  // write magic and version
  os.write(reinterpret_cast<const char*>(&g_magic), sizeof(g_magic));
  os.write(reinterpret_cast<const char*>(&g_version), sizeof(g_version));

  // write each child
  for (const auto& child : inode) {
    if (child->is_ignored()) {
      continue;
    }

    // Write the path
    auto path = child->name();
    uint64_t path_length = path.length();
    os.write(reinterpret_cast<const char*>(&path_length), sizeof(path_length));
    os.write(path.c_str(), path.length());

    // Write the hash
    auto& hash = child->hash();
    uint64_t hash_length = hash.size();
    os.write(reinterpret_cast<const char*>(&hash_length), sizeof(hash_length));
    os.write(hash.c_str(), hash.size());

    // Write the status bits
    uint32_t status_bits = child->status();
    os.write(reinterpret_cast<const char*>(&status_bits), sizeof(status_bits));

    // Write the target if it's a symlink
    if (child->is_symlink()) {
      auto& target = child->target();
      uint64_t target_length = target.size();
      os.write(reinterpret_cast<const char*>(&target_length), sizeof(target_length));
      os.write(target.c_str(), target.size());
    }

    // Check if there was an error writing
    if (!os) {
      break;
    }
  }

  if (!os) {
    throw std::runtime_error("failed writing tree: " + inode.hash() + ": " + std::strerror(errno));
  }

  return os;
}

std::istream& operator>>(std::istream& is, inode& inode) {
  // read magic and version

  uint16_t magic;
  is.read(reinterpret_cast<char*>(&magic), sizeof(magic));
  if (!is) throw std::runtime_error("failed reading tree: " + inode.hash() + ": " + std::strerror(errno));

  if (magic != g_magic) {
    throw std::runtime_error("failed reading tree: " + inode.hash() + ": invalid magic");
  }

  uint16_t version;
  is.read(reinterpret_cast<char*>(&version), sizeof(version));
  if (!is) throw std::runtime_error("failed reading tree: " + inode.hash() + ": " + std::strerror(errno));

  if (version != g_version) {
    throw std::runtime_error("failed reading tree: " + inode.hash() + ": unsupported version");
  }

  while (is.peek() != EOF) {
    // Read the path
    std::string path;
    uint64_t path_length;
    is.read(reinterpret_cast<char*>(&path_length), sizeof(path_length));
    if (!is) throw std::runtime_error("failed reading tree: " + inode.hash() + ": " + std::strerror(errno));

    path.resize(path_length);
    is.read(&path[0], path_length);
    if (!is) throw std::runtime_error("failed reading tree: " + inode.hash() + ": " + std::strerror(errno));

    // Read the hash
    std::string hash;
    uint64_t hash_length;
    is.read(reinterpret_cast<char*>(&hash_length), sizeof(hash_length));
    if (!is) throw std::runtime_error("failed reading tree: " + inode.hash() + ": " + std::strerror(errno));

    hash.resize(hash_length);
    is.read(&hash[0], hash_length);
    if (!is) throw std::runtime_error("failed reading tree: " + inode.hash() + ": " + std::strerror(errno));

    // Read the file status
    uint32_t status_bits;
    is.read(reinterpret_cast<char*>(&status_bits), sizeof(status_bits));
    if (!is) throw std::runtime_error("failed reading tree: " + inode.hash() + ": " + std::strerror(errno));

    auto status = static_cast<file_status>(status_bits);

    // // Read the mtime
    // uint64_t mtime;
    // is.read(reinterpret_cast<char *>(&mtime), sizeof(mtime));

    // Read the target if it's a symlink
    std::string target;
    if (status.is_symlink()) {
      uint64_t target_length;
      is.read(reinterpret_cast<char*>(&target_length), sizeof(target_length));
      if (!is) throw std::runtime_error("failed reading tree: " + inode.hash() + ": " + std::strerror(errno));

      target.resize(target_length);
      is.read(&target[0], target_length);
      if (!is) throw std::runtime_error("failed reading tree: " + inode.hash() + ": " + std::strerror(errno));
    }

    std::filesystem::path inode_path = inode.path();
    inode_path /= path;
    inode.add_child(new fstree::inode(inode_path.string(), status, inode::time_type(0), 0ul, target, hash));
  }

  return is;
}

}  // namespace fstree
