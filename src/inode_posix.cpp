#include "inode.hpp"

bool inode::is_directory() const { return (_mode & S_IFMT) == S_IFDIR; }
bool inode::is_file() const { return (_mode & S_IFMT) == S_IFREG; }
bool inode::is_symlink() const { return (_mode & S_IFMT) == S_IFLNK; }

inline std::ostream& operator<<(std::ostream& os, const inode& inode) {
  // write each child
  for (const auto& child : inode) {
    // Write the path
    auto path = child->name();
    size_t path_length = path.length();
    os.write(reinterpret_cast<const char*>(&path_length), sizeof(path_length));
    os.write(path.c_str(), path.length());

    // Write the hash
    auto& hash = child->hash();
    size_t hash_length = hash.size();
    os.write(reinterpret_cast<const char*>(&hash_length), sizeof(hash_length));
    os.write(hash.c_str(), hash.size());

    // Write the inode number
    auto ino = child->ino();
    os.write(reinterpret_cast<const char*>(&ino), sizeof(ino));

    // Write the mode
    auto mode = child->mode();
    os.write(reinterpret_cast<const char*>(&mode), sizeof(mode));

    // Write the target if it's a symlink
    if (child->is_symlink()) {
      auto& target = child->target();
      auto target_length = target.size();
      os.write(reinterpret_cast<const char*>(&target_length), sizeof(target_length));
      os.write(target.c_str(), target.size());
    }
  }

  return os;
}

inline std::istream& operator>>(std::istream& is, inode& inode) {
  while (is.peek() != EOF) {
    // Read the path
    std::string path;
    size_t path_length;
    is.read(reinterpret_cast<char*>(&path_length), sizeof(path_length));
    path.resize(path_length);
    is.read(&path[0], path_length);

    // Read the hash
    std::string hash;
    size_t hash_length;
    is.read(reinterpret_cast<char*>(&hash_length), sizeof(hash_length));
    hash.resize(hash_length);
    is.read(&hash[0], hash_length);

    // Read the inode number
    ino64_t ino;
    is.read(reinterpret_cast<char*>(&ino), sizeof(ino));

    // Read the mode
    ::inode::mode_type mode;
    is.read(reinterpret_cast<char*>(&mode), sizeof(mode));

    // // Read the mtime
    // uint64_t mtime;
    // is.read(reinterpret_cast<char *>(&mtime), sizeof(mtime));

    // Read the target if it's a symlink
    std::string target;
    if ((mode & S_IFLNK) == S_IFLNK) {
      size_t target_length;
      is.read(reinterpret_cast<char*>(&target_length), sizeof(target_length));
      target.resize(target_length);
      is.read(&target[0], target_length);
    }

    inode.add_child(new ::inode(fs::path(inode.path()) / path, ino, mode, 0, target, hash));
  }
  return is;
}
