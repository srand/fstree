
#include "index.hpp"

#include "cache.hpp"
#include "directory_iterator.hpp"
#include "event.hpp"
#include "filesystem.hpp"
#include "inode.hpp"
#include "sha1.hpp"
#include "wait_group.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <future>
#include <iostream>
#include <thread>
#include <vector>

namespace fstree {

static const uint16_t magic = 0x3ee3;
static const uint16_t version = 1;

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

    push_back(new fstree::inode(path, status, mtime, 0, target, hash));
  }
}

void index::refresh() {
  event("index::refresh", _root_path.string());

  sorted_directory_iterator tree(_root_path, _ignore);

  auto cur_tree_node = tree.begin();
  auto cur_index_node = _inodes.begin();
  auto end_tree_node = tree.end();
  auto end_index_node = _inodes.end();

  index new_index;
  std::vector<inode*> rehash_inodes;

  for (;;) {
    // Reached the end of the tree. All remaining index nodes are removed.
    if (cur_tree_node == end_tree_node) {
      break;
    }

    // Reached the end of the index. All remaining tree nodes are added.
    if (cur_index_node == end_index_node) {
      std::for_each(cur_tree_node, end_tree_node, [&](auto& inode) {
        inode->set_dirty();
        new_index.push_back(inode);
      });
      break;
    }

    // Compare the current nodes

    // If the tree node is less than the index node, it's added
    if ((*cur_tree_node)->path() < (*cur_index_node)->path()) {
      (*cur_tree_node)->set_dirty();
      new_index.push_back(*cur_tree_node);
      cur_tree_node++;
      continue;
    }

    // If the tree node is greater than the index node, it's removed
    if ((*cur_tree_node)->path() > (*cur_index_node)->path()) {
      cur_index_node++;
      continue;
    }

    // If the tree node is equal to the index node, it's maybe modified
    if ((*cur_tree_node)->path() == (*cur_index_node)->path()) {
      // Compare inode type
      if ((*cur_tree_node)->type() != (*cur_index_node)->type()) {
        (*cur_tree_node)->set_dirty();
        new_index.push_back(*cur_tree_node);
      }

      // Compare inode permissions
      else if ((*cur_tree_node)->permissions() != (*cur_index_node)->permissions()) {
        (*cur_tree_node)->set_dirty();
        new_index.push_back(*cur_tree_node);
      }

      // Compare modification time
      else if ((*cur_tree_node)->last_write_time() != (*cur_index_node)->last_write_time()) {
        (*cur_tree_node)->set_dirty();
        new_index.push_back(*cur_tree_node);
      }

      else {
        (*cur_tree_node)->set_hash((*cur_index_node)->hash());
        new_index.push_back(*cur_tree_node);
      }

      cur_tree_node++;
      cur_index_node++;
      continue;
    }
  }

  _inodes = std::move(new_index._inodes);
  _root = std::move(tree.root());
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
        std::filesystem::remove_all(path / (*cur_tree_node)->path(), ec);
        if (ec) {
          throw std::runtime_error("failed to remove file: " + path.string() + ": " + ec.message());
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

      std::filesystem::remove_all(path / (*cur_tree_node)->path());
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
          case std::filesystem::file_type::directory:
            std::filesystem::remove_all(path / (*cur_tree_node)->path(), ec);
            if (ec) {
              throw std::runtime_error("failed to remove directory: " + path.string() + ": " + ec.message());
            }
            break;
          default:
            std::filesystem::remove(path / (*cur_tree_node)->path(), ec);
            if (ec) {
              throw std::runtime_error("failed to remove file: " + path.string() + ": " + ec.message());
            }
            break;
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

void index::checkout_node(fstree::cache& c, inode* node, const std::filesystem::path& path) {
  std::filesystem::path full_path = path / node->path();
  std::error_code ec;

  if (node->is_symlink()) {
    std::filesystem::remove(full_path, ec);
    if (ec) {
      throw std::runtime_error("failed to remove symlink: " + full_path.string() + ": " + ec.message());
    }
    std::filesystem::create_symlink(node->target(), full_path, ec);
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
    c.copy(node->hash(), full_path);
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

}  // namespace fstree
