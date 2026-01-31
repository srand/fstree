#include "index.hpp"

#include "cache.hpp"
#include "directory_iterator.hpp"
#include "event.hpp"
#include "filesystem.hpp"
#include "hash.hpp"
#include "inode.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

namespace fstree {

static const uint16_t magic = 0x3ee3;
static const uint16_t version = 1;

// Constructor implementations
index::index() 
  : _root(fstree::make_intrusive<fstree::inode>()) 
{}

index::index(const std::filesystem::path& root) 
  : _root_path(root), _root(fstree::make_intrusive<fstree::inode>())
{}

index::index(const std::filesystem::path& root, const glob_list& ignore) 
  : _ignore(ignore)
  , _root_path(root)
  , _root(fstree::make_intrusive<fstree::inode>()) 
{}

index::~index() {
  if (_root) {
    _root->clear();
  }
  std::for_each(_inodes.begin(), _inodes.end(), [](inode::ptr& inode) {
    inode->clear();
  });
}

void index::dump() const {
  for (const auto& inode : _inodes) {
    std::cerr << std::setw(40) << inode->hash() << " " << inode->path() << std::endl;
    if (inode->is_directory()) {
      for (const auto& child : *inode) {
        std::cerr << "  " << std::setw(40) << child->hash() << " " << child->path() << std::endl;
      }
    }
  }
  std::cerr << std::endl;
}

// Iterator methods
std::vector<inode::ptr>::iterator index::begin() { return _inodes.begin(); }

std::vector<inode::ptr>::iterator index::end() { return _inodes.end(); }

std::vector<inode::ptr>::const_iterator index::begin() const { return _inodes.cbegin(); }

std::vector<inode::ptr>::const_iterator index::end() const { return _inodes.cend(); }

std::vector<inode::ptr>::size_type index::size() const { return _inodes.size(); }

std::string index::root_path() const { return _root_path.string(); }

inode::ptr& index::root() { return _root; }

const inode::ptr& index::root() const { return _root; }

void index::push_back(inode::ptr inode) { _inodes.push_back(inode); }

void index::save() const {
  save(std::filesystem::path(".fstree/index"));
}

// Serializes the index to a file using run length encoding
void index::save(const std::filesystem::path& indexfile) const {
  const std::filesystem::path& path = _root_path / indexfile;
  event("index::save", path.string());

  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  if (ec) {
    throw std::runtime_error("failed to create index directory: " + path.parent_path().string() + ": " + ec.message());
  }

  std::ofstream file(path, std::ios::trunc | std::ios::binary);
  if (!file) {
    throw std::runtime_error("failed to open index for writing: " + path.string() + ": " + std::strerror(errno));
  }

  // Write magic and version
  file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
  file.write(reinterpret_cast<const char*>(&version), sizeof(version));

  for (const auto& inode : *this) {
    // Write the path
    size_t path_length = inode->path().length();
    file.write(reinterpret_cast<const char*>(&path_length), sizeof(path_length));
    file.write(inode->path().c_str(), inode->path().length());

    // Write the hash
    size_t hash_length = inode->hash().size();
    file.write(reinterpret_cast<const char*>(&hash_length), sizeof(hash_length));
    file.write(inode->hash().c_str(), inode->hash().size());

    // Write status bits
    uint32_t status_bits = inode->status();
    file.write(reinterpret_cast<const char*>(&status_bits), sizeof(status_bits));

    // Write mtime
    auto mtime = inode->last_write_time();
    file.write(reinterpret_cast<const char*>(&mtime), sizeof(mtime));

    if (inode->is_symlink()) {
      size_t target_length = inode->target().length();
      file.write(reinterpret_cast<const char*>(&target_length), sizeof(target_length));
      file.write(inode->target().c_str(), inode->target().length());
    }

    if (!file) break;
  }

  if (!file) throw std::runtime_error("failed writing index: " + path.string() + ": " + std::strerror(errno));
}

void index::load() {
  load(std::filesystem::path(".fstree/index"));
}

// Deserializes the index from a file using run length encoding
void index::load(const std::filesystem::path& indexfile) {
  const std::filesystem::path& index_path = _root_path / indexfile;
  event("index::load", index_path.string());

  std::ifstream file(index_path, std::ios::binary);
  if (!file) {
    throw std::runtime_error("failed to open index for reading: " + index_path.string() + ": " + std::strerror(errno));
  }

  uint16_t file_magic;
  file.read(reinterpret_cast<char*>(&file_magic), sizeof(file_magic));
  if (!file) throw std::runtime_error("failed reading index: " + index_path.string() + ": " + std::strerror(errno));
  if (file_magic != magic) throw std::runtime_error("failed reading index: " + index_path.string() + ": invalid magic");

  uint16_t file_version;
  file.read(reinterpret_cast<char*>(&file_version), sizeof(file_version));
  if (!file) throw std::runtime_error("failed reading index: " + index_path.string() + ": " + std::strerror(errno));
  if (file_version != version)
    throw std::runtime_error("failed reading index: " + index_path.string() + ": invalid version");

  _inodes.clear();

  while (file.peek() != EOF) {
    std::string path;
    size_t path_length;
    file.read(reinterpret_cast<char*>(&path_length), sizeof(path_length));
    if (!file) throw std::runtime_error("failed reading index: " + index_path.string() + ": " + std::strerror(errno));

    path.resize(path_length);
    file.read(&path[0], path_length);
    if (!file) throw std::runtime_error("failed reading index: " + index_path.string() + ": " + std::strerror(errno));

    std::string hash;
    size_t hash_length;
    file.read(reinterpret_cast<char*>(&hash_length), sizeof(hash_length));
    if (!file) throw std::runtime_error("failed reading index: " + index_path.string() + ": " + std::strerror(errno));

    hash.resize(hash_length);
    file.read(&hash[0], hash_length);
    if (!file) throw std::runtime_error("failed reading index: " + index_path.string() + ": " + std::strerror(errno));

    uint32_t status_bits;
    file.read(reinterpret_cast<char*>(&status_bits), sizeof(status_bits));
    if (!file) throw std::runtime_error("failed reading index: " + index_path.string() + ": " + std::strerror(errno));
    auto status = static_cast<file_status>(status_bits);

    // Microseconds since epoch
    inode::time_type mtime;
    file.read(reinterpret_cast<char*>(&mtime), sizeof(mtime));
    if (!file) throw std::runtime_error("failed reading index: " + index_path.string() + ": " + std::strerror(errno));

    std::string target;
    if (status.is_symlink()) {
      size_t target_length;
      file.read(reinterpret_cast<char*>(&target_length), sizeof(target_length));
      if (!file) throw std::runtime_error("failed reading index: " + index_path.string() + ": " + std::strerror(errno));

      target.resize(target_length);
      file.read(&target[0], target_length);
      if (!file) throw std::runtime_error("failed reading index: " + index_path.string() + ": " + std::strerror(errno));
    }

    push_back(fstree::make_intrusive<fstree::inode>(path, status, mtime, 0, target, hash));
  }
}

void index::copy_metadata(fstree::index& other) {
  auto cur_index_node = _inodes.begin();
  auto cur_other_node = other._inodes.begin();
  auto end_index_node = _inodes.end();
  auto end_other_node = other._inodes.end();

  for (;;) {
    if (cur_index_node == end_index_node) {
      return;
    }

    if (cur_other_node == end_other_node) {
      return;
    }

    if ((*cur_index_node)->path() < (*cur_other_node)->path()) {
      cur_index_node++;
      continue;
    }

    if ((*cur_index_node)->path() > (*cur_other_node)->path()) {
      cur_other_node++;
      continue;
    }

    if ((*cur_index_node)->hash() == (*cur_other_node)->hash()) {
      (*cur_index_node)->set_last_write_time((*cur_other_node)->last_write_time());
    }

    cur_index_node++;
    cur_other_node++;
  }
}

void index::sort() {
  // Sort inodes by path
  std::sort(_inodes.begin(), _inodes.end(), [](const auto& a, const auto& b) { return a->path() < b->path(); });
}

void index::checkout(fstree::cache& cache, const std::filesystem::path& path) {
  event("index::checkout", path.string());

  // Create destination directory if it doesn't exist
  std::error_code ec;
  std::filesystem::create_directories(path, ec);
  if (ec) {
    throw std::runtime_error("failed to create directory: " + path.string() + ": " + ec.message());
  }

  sorted_directory_iterator tree(path, _ignore);

  auto cur_tree_node = tree.begin();
  auto cur_index_node = _inodes.begin();
  auto end_tree_node = tree.end();
  auto end_index_node = _inodes.end();

  index new_index;

  for (;;) {
    // Reached the end of the tree. All remaining index nodes are added.
    if (cur_tree_node == end_tree_node) {
      for (; cur_index_node != end_index_node; cur_index_node++) {
        checkout_node(cache, *cur_index_node, path);
      }
      break;
    }

    // Reached the end of the index. All remaining tree nodes should be removed.
    if (cur_index_node == end_index_node) {
      for (; cur_tree_node != end_tree_node; cur_tree_node++) {
        // Remove files that are not in the index
        std::filesystem::path absolute_path = path / (*cur_tree_node)->path();
        std::filesystem::remove_all(absolute_path, ec);
        if (ec) {
          throw std::runtime_error("failed to remove file: " + absolute_path.string() + ": " + ec.message());
        }
      }
      break;
    }

    // If the tree node is less than the index node, it should be removed
    if ((*cur_tree_node)->path() < (*cur_index_node)->path()) {
      // Check if the parent directory of the tree node is canonical.
      // If not, the tree node must be ignored because it's parent directory became a symlink.
      std::filesystem::path tree_parent = (path / (*cur_tree_node)->path()).parent_path();
      std::filesystem::path tree_canonical_parent = std::filesystem::weakly_canonical(tree_parent);
      if (tree_parent != tree_canonical_parent) {
        cur_tree_node++;
        continue;
      }

      std::filesystem::path absolute_path = path / (*cur_tree_node)->path();
      std::filesystem::remove_all(absolute_path, ec);
      if (ec) {
        throw std::runtime_error("failed to remove directory: " + absolute_path.string() + ": " + ec.message());
      }
      cur_tree_node++;
      continue;
    }

    // If the tree node is greater than the index node, index node should be created.
    if ((*cur_tree_node)->path() > (*cur_index_node)->path()) {
      checkout_node(cache, *cur_index_node, path);
      cur_index_node++;
      continue;
    }

    // If the tree node is equal to the index node, it's maybe modified
    if ((*cur_tree_node)->path() == (*cur_index_node)->path()) {
      // Compare inode type
      if ((*cur_tree_node)->type() != (*cur_index_node)->type()) {
        switch ((*cur_tree_node)->type()) {
          case std::filesystem::file_type::directory: {
            std::filesystem::path absolute_path = path / (*cur_tree_node)->path();
            std::filesystem::remove_all(absolute_path, ec);
            if (ec) {
              throw std::runtime_error("failed to remove directory: " + absolute_path.string() + ": " + ec.message());
            }
            // Skip ahead all children of the directory in the tree
            {
              auto dir_path = (*cur_tree_node)->path() + "/";
              cur_tree_node++;
              while (cur_tree_node != end_tree_node && (*cur_tree_node)->path().find(dir_path) == 0) {
                cur_tree_node++;
              }
            }
            checkout_node(cache, *cur_index_node, path);
            cur_index_node++;
            continue;
          }

          default: {
            std::filesystem::path absolute_path = path / (*cur_tree_node)->path();
            std::filesystem::remove(absolute_path, ec);
            if (ec) {
              throw std::runtime_error("failed to remove file: " + absolute_path.string() + ": " + ec.message());
            }
            break;
          }
        }
        checkout_node(cache, *cur_index_node, path);

        cur_tree_node++;
        cur_index_node++;
        continue;
      }

      // Compare modification time
      if ((*cur_tree_node)->last_write_time() != (*cur_index_node)->last_write_time()) {
        if (!(*cur_index_node)->is_directory()) checkout_node(cache, *cur_index_node, path);
      }

      // Compare inode permissions
      if ((*cur_tree_node)->permissions() != (*cur_index_node)->permissions()) {
        std::filesystem::permissions(
            path / (*cur_index_node)->path(), (*cur_index_node)->permissions(), std::filesystem::perm_options::replace,
            ec);
        if (ec) {
          throw std::runtime_error("failed to set permissions: " + path.string() + ": " + ec.message());
        }
      }

      // Compare symlink target
      if ((*cur_index_node)->target() != (*cur_tree_node)->target()) {
        std::filesystem::remove(path / (*cur_index_node)->path(), ec);
        if (ec) {
          throw std::runtime_error("failed to remove symlink: " + path.string() + ": " + ec.message());
        }

        checkout_node(cache, *cur_index_node, path);
      }

      cur_tree_node++;
      cur_index_node++;
      continue;
    }
  }
}

void index::checkout_node(fstree::cache& c, inode::ptr node, const std::filesystem::path& path) {
  std::filesystem::path full_path = path / node->path();
  std::error_code ec;

  if (node->is_symlink()) {
    std::filesystem::remove(full_path, ec);
    if (ec) {
      throw std::runtime_error("failed to remove symlink: " + full_path.string() + ": " + ec.message());
    }
    std::filesystem::create_symlink(node->target_path(), full_path, ec);
    if (ec) {
      throw std::runtime_error("failed to create symlink: " + full_path.string() + ": " + ec.message());
    }
#ifdef _WIN32
    std::filesystem::permissions(
        full_path, node->permissions(),
        std::filesystem::perm_options::replace | std::filesystem::perm_options::nofollow, ec);
    if (ec) {
      throw std::runtime_error("failed to set permissions: " + full_path.string() + ": " + ec.message());
    }
#endif
  }
  else if (node->is_directory()) {
    std::filesystem::create_directory(full_path, ec);
    if (ec) {
      throw std::runtime_error("failed to create directory: " + full_path.string() + ": " + ec.message());
    }
    std::filesystem::permissions(full_path, node->permissions(), std::filesystem::perm_options::replace, ec);
    if (ec) {
      throw std::runtime_error("failed to set permissions: " + full_path.string() + ": " + ec.message());
    }
  }
  else if (node->is_file()) {
    std::filesystem::remove(full_path, ec);
    if (ec) {
      throw std::runtime_error("failed to remove file: " + full_path.string() + ": " + ec.message());
    }
    c.copy_file(node->hash(), full_path);
    std::filesystem::permissions(full_path, node->permissions(), std::filesystem::perm_options::replace, ec);
    if (ec) {
      throw std::runtime_error("failed to set permissions: " + full_path.string() + ": " + ec.message());
    }
  }

  // Stat new file and update inode
  fstree::stat st;
  lstat(full_path.c_str(), st);

  node->set_last_write_time(st.last_write_time);
  node->set_status(st.status);
}

inode::ptr index::find_node_by_path(const std::filesystem::path& path) {
  auto it = std::lower_bound(_inodes.begin(), _inodes.end(), path.string(), [](const inode::ptr& a, const std::string& b) {
    return a->path() < b;
  });
  if (it != _inodes.end() && (*it)->path() == path) {
    return *it;
  }
  return nullptr;
}

void index::load_ignore_from_index(fstree::cache& cache, const std::filesystem::path& path) {
  fstree::inode::ptr ignore_node = find_node_by_path(path);
  if (ignore_node && ignore_node->is_file()) {
    auto ignore_path = cache.file_path(ignore_node);
    _ignore.load(ignore_path.string());
  }
}

void index::refresh() {
  event("index::refresh", _root_path.string());

  // Our goal is to scan the filesystem tree and replace the index with it,
  // However, we want to preserve hashes from the index where possible to avoid
  // re-hashing unchanged files.

  // Note that the index tree may be incomplete and lacks parent/child 
  // relationships. We can only rely on the list of inodes in the index.

  // Scan the filesystem tree
  sorted_directory_iterator tree(_root_path, _ignore);

  // Copy index a temporary vector and clear the index
  std::vector<inode::ptr> nodes(std::move(_inodes));

  auto tree_it = tree.begin();
  auto tree_end = tree.end();
  auto index_it = nodes.begin();
  auto index_end = nodes.end();

  // Iterate through the filesystem tree and the index in parallel
  // looking for matching paths.

  while (tree_it != tree_end || index_it != index_end) {
    if (tree_it == tree_end) {
      // No more tree nodes - done
      break;
    }
    else if (index_it == index_end) {
      // No more index nodes - add remaining tree nodes
      for (; tree_it != tree_end; ++tree_it) {
        _inodes.push_back(*tree_it);
      }
      break;
    }
    else if ((*tree_it)->path() < (*index_it)->path()) {
      // Tree node is new - add it
      _inodes.push_back(*tree_it);
      ++tree_it;
    }
    else if ((*tree_it)->path() > (*index_it)->path()) {
      // Index node was deleted - ignore it
      ++index_it;
    }
    else {
      // Matching paths - keep the tree node
      _inodes.push_back(*tree_it);

      // Check if hash can be reused from index
      // It's reused if the inodes have the same metadata.
      // The hash length must also match the current algorithm's hash length.
      if ((*index_it)->hash().length() != fstree::hash_digest_len) {
        (*tree_it)->set_dirty();
      }
      else if ((*index_it)->is_equivalent(*tree_it)) {
        (*tree_it)->set_hash((*index_it)->hash());
      } else {
        (*tree_it)->set_dirty();
      }

      ++tree_it;
      ++index_it;
    }
  }

  // Replace root node
  _root = std::move(tree.root());
}

} // namespace fstree
