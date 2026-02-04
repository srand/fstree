#include "inode.hpp"
#include "hash.hpp"

#include <algorithm>
#include <cstring>

namespace fstree {

static const uint16_t g_magic = 0x3eee;
static const uint16_t g_version = 1;

// Constructor implementations
inode::inode() : _status(file_status(std::filesystem::file_type::directory, std::filesystem::perms::none)) {}

inode::inode(
    const std::string& path,
    file_status status,
    time_type mtime,
    size_t size,
    const std::string& target,
    const fstree::digest& hash)
    : _path(path), _hash(hash), _status(status), _last_write_time(mtime), _size(size), _target(target) {}

void inode::add_child(inode::ptr& child) {
  _children.push_back(child);
  child->set_parent(intrusive_from_this());
}

// Iterator implementations
std::vector<inode::ptr>::iterator inode::begin() { return _children.begin(); }

std::vector<inode::ptr>::iterator inode::end() { return _children.end(); }

std::vector<inode::ptr>::const_iterator inode::begin() const { return _children.begin(); }

std::vector<inode::ptr>::const_iterator inode::end() const { return _children.end(); }

// Type checking methods
bool inode::is_directory() const { return _status.is_directory(); }

bool inode::is_file() const { return _status.is_regular(); }

bool inode::is_symlink() const { return _status.is_symlink(); }

// Hash methods
const fstree::digest& inode::hash() const { return _hash; }

void inode::set_hash(const fstree::digest& hash) { _hash = hash; }

// Status methods
file_status inode::status() const { return _status; }

void inode::set_status(file_status status) { _status = status; }

// Size method
size_t inode::size() const { return _size; }

// Type and permissions
std::filesystem::file_type inode::type() const { return _status.type(); }

std::filesystem::perms inode::permissions() const { return _status.permissions(); }

// Time methods
inode::time_type inode::last_write_time() const { return _last_write_time; }

void inode::set_last_write_time(time_type last_write_time) { _last_write_time = last_write_time; }

// Target methods
const std::string& inode::target() const { return _target; }

std::filesystem::path inode::target_path() const { return std::filesystem::path(_target).make_preferred(); }

// Path methods
const std::string& inode::path() const { return _path; }

std::string inode::name() const { return std::filesystem::path(_path).filename().string(); }

const inode::ptr& inode::parent() const { return _parent; }

void inode::set_parent(const inode::ptr& parent) { _parent = parent; }

void inode::set_dirty() {
  _hash = digest();
  if (_parent && !_parent->is_dirty()) {
    _parent->set_dirty();
  }
}

void inode::sort() {
  std::sort(_children.begin(), _children.end(), [](const inode::ptr& a, const inode::ptr& b) { return a->path() < b->path(); });
}

bool inode::is_dirty() const { return _hash.empty(); }

bool inode::is_equivalent(const inode::ptr& other) const {
  return _path == other->_path && _status.type() == other->_status.type() &&
         _status.permissions() == other->_status.permissions() && _last_write_time == other->_last_write_time &&
         _target == other->_target;
}

// Equality operator
bool inode::operator==(const inode& other) const {
  return _path == other._path && _status.type() == other._status.type() &&
         _status.permissions() == other._status.permissions() && _last_write_time == other._last_write_time &&
         _target == other._target;
}

// Ignore methods
void inode::ignore() { _ignored = true; }

bool inode::is_ignored() const { return _ignored; }

void inode::unignore() {
  if (!_unignored) {
    _unignored = true;
    if (_parent) _parent->unignore();
  }
}

bool inode::is_unignored() const { return _unignored; }

void inode::rehash(const std::filesystem::path& root) { 
  _hash = hashsum_hex_file(root / _path); 
}

void inode::clear() {
  std::for_each(_children.begin(), _children.end(), [](inode::ptr& child) {
    child->clear();
  });
  _children.clear();
  _parent.reset();
}

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
    auto hash = child->hash().string();
    uint64_t hash_length = hash.length();
    os.write(reinterpret_cast<const char*>(&hash_length), sizeof(hash_length));
    os.write(hash.c_str(), hash_length);

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
    throw std::runtime_error("failed writing tree: " + inode.hash().string() + ": " + std::strerror(errno));
  }

  return os;
}

std::istream& operator>>(std::istream& is, inode& inode) {
  // read magic and version

  uint16_t magic;
  is.read(reinterpret_cast<char*>(&magic), sizeof(magic));
  if (!is) throw std::runtime_error("failed reading tree: " + inode.hash().string() + ": " + std::strerror(errno));

  if (magic != g_magic) {
    throw std::runtime_error("failed reading tree: " + inode.hash().string() + ": invalid magic");
  }

  uint16_t version;
  is.read(reinterpret_cast<char*>(&version), sizeof(version));
  if (!is) throw std::runtime_error("failed reading tree: " + inode.hash().string() + ": " + std::strerror(errno));

  if (version != g_version) {
    throw std::runtime_error("failed reading tree: " + inode.hash().string() + ": unsupported version");
  }

  while (is.peek() != EOF) {
    // Read the path
    std::string path;
    uint64_t path_length;
    is.read(reinterpret_cast<char*>(&path_length), sizeof(path_length));
    if (!is) throw std::runtime_error("failed reading tree: " + inode.hash().string() + ": " + std::strerror(errno));

    path.resize(path_length);
    is.read(&path[0], path_length);
    if (!is) throw std::runtime_error("failed reading tree: " + inode.hash().string() + ": " + std::strerror(errno));
    // Read the hash
    std::string hash;
    uint64_t hash_length;
    is.read(reinterpret_cast<char*>(&hash_length), sizeof(hash_length));
    if (!is) throw std::runtime_error("failed reading tree: " + inode.hash().string() + ": " + std::strerror(errno));

    hash.resize(hash_length);
    is.read(&hash[0], hash_length);
    if (!is) throw std::runtime_error("failed reading tree: " + inode.hash().string() + ": " + std::strerror(errno));
    // Read the file status
    uint32_t status_bits;
    is.read(reinterpret_cast<char*>(&status_bits), sizeof(status_bits));
    if (!is) throw std::runtime_error("failed reading tree: " + inode.hash().string() + ": " + std::strerror(errno));

    auto status = static_cast<file_status>(status_bits);

    // // Read the mtime
    // uint64_t mtime;
    // is.read(reinterpret_cast<char *>(&mtime), sizeof(mtime));

    // Read the target if it's a symlink
    std::string target;
    if (status.is_symlink()) {
      uint64_t target_length;
      is.read(reinterpret_cast<char*>(&target_length), sizeof(target_length));
      if (!is) throw std::runtime_error("failed reading tree: " + inode.hash().string() + ": " + std::strerror(errno));

      target.resize(target_length);
      is.read(&target[0], target_length);
      if (!is) throw std::runtime_error("failed reading tree: " + inode.hash().string() + ": " + std::strerror(errno));
    }

    std::filesystem::path inode_path = inode.path();
    inode_path /= path;
    auto child = fstree::make_intrusive<fstree::inode>(
      inode_path.string(), status, inode::time_type(0), 0ul, target, fstree::digest::parse(hash));
    inode.add_child(child);
  }

  return is;
}

}  // namespace fstree
