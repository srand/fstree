#include "cache.hpp"

#include "directory_iterator.hpp"
#include "event.hpp"
#include "exception.hpp"
#include "filesystem.hpp"
#include "hash.hpp"
#include "inode.hpp"
#include "thread_pool.hpp"
#include "wait_group.hpp"

#include <cstring>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

namespace fstree {

std::filesystem::path cache::default_path() { return fstree::cache_path(); }

cache::cache()
    : _objectdir(default_path() / "objects"),
      _tmpdir(default_path() / "tmp"),
      _max_size(default_max_size),
      _max_size_slice(default_max_size >> 8),
      _retention_period(default_retention),
      _lock(default_path() / "objects" / "lock") {
  std::error_code ec;

  std::filesystem::create_directories(_objectdir, ec);
  if (ec) {
    throw std::runtime_error("failed to create cache object directory: " + _objectdir.string() + ": " + ec.message());
  }

  std::filesystem::create_directories(_tmpdir, ec);
  if (ec) {
    throw std::runtime_error("failed to create cache temporary directory: " + _tmpdir.string() + ": " + ec.message());
  }
}

cache::cache(const std::filesystem::path& path, size_t max_size, std::chrono::seconds retention_period)
    : _objectdir(path / "objects"),
      _tmpdir(path / "tmp"),
      _max_size(max_size),
      _max_size_slice(max_size >> 8),
      _retention_period(retention_period),
      _lock(path / "objects" / "lock") {
  std::error_code ec;

  std::filesystem::create_directories(_objectdir, ec);
  if (ec) {
    throw std::runtime_error("failed to create cache object directory: " + _objectdir.string() + ": " + ec.message());
  }

  std::filesystem::create_directories(_tmpdir, ec);
  if (ec) {
    throw std::runtime_error("failed to create cache temporary directory: " + _tmpdir.string() + ": " + ec.message());
  }
}

void cache::add(fstree::index& index) {
  event("cache::add", index.root_path());

  pool& pool = get_pool();
  wait_group wg;

  // List of dirty directory inodes
  std::vector<inode::ptr> dirty_dirs;

  for (const auto& inode : index) {
    if (inode->is_file()) {
      wg.add(1);
      pool.enqueue([this, &index, inode, &wg]() {
        try {
          std::error_code ec;

          if (inode->is_dirty()) {
            inode->rehash(index.root_path());
            auto context = _lock.lock();
            if (!has_object(inode->hash())) {
              event("cache::add", inode->path(), "dirty");
              create_file(index.root_path(), inode);
            }
          }
          else {
            auto context = _lock.lock();
            if (!has_object(inode->hash())) {
              event("cache::add", inode->path(), "missing");
              create_file(index.root_path(), inode);
            }
          }

          wg.done();
        }
        catch (const std::exception& e) {
          wg.exception(e);
        }
      });
    }
    else if (inode->is_directory()) {
      if (inode->is_dirty()) {
        dirty_dirs.push_back(inode);
      }
      else if (!has_tree(inode->hash())) {
        dirty_dirs.push_back(inode);
      }
    }
  }

  wg.wait_rethrow();

#ifdef _WIN32
  auto context = _lock.lock();
#endif

  // Create directory nodes in reverse order
  for (auto it = dirty_dirs.rbegin(); it != dirty_dirs.rend(); ++it) {
    if ((*it)->is_dirty()) {
      event("cache::add", (*it)->path(), "dirty");
    }
    else {
      event("cache::add", (*it)->path(), "missing");
    }

    create_dirtree(*it);
  }

  create_dirtree(index.root());
}

void cache::read_tree(const fstree::digest& hash, inode::ptr& inode) {
  std::error_code ec;

#ifdef _WIN32
  auto lock = _lock.lock();
#endif

  inode->set_hash(hash);

  std::filesystem::path object = tree_path(inode);
  if (!std::filesystem::exists(object, ec)) {
    throw std::runtime_error("tree object not found in local cache: " + hash.string());
  }

  std::ifstream file(object, std::ios::binary);
  if (!file) {
    throw std::runtime_error("failed to open tree object: " + object.string() + ": " + ec.message());
  }

  file >> *inode;
}

void cache::index_from_tree(const fstree::digest& hash, fstree::index& index) {
  std::vector<inode::ptr> trees;
  pool& pool = get_pool();
  std::mutex mutex;
  wait_group wg;

  // Add root directory
  index.root()->set_hash(hash);
  index.root()->set_status(fstree::file_status(fs::file_type::directory, fs::perms::none));
  trees.push_back(index.root());

  // Recursively read tree objects in parallel
  while (!trees.empty()) {
    std::vector<inode::ptr> new_trees;

    // For each tree, read its content and add child directories to the list of trees to read.
    for (auto& tree : trees) {
      wg.add(1);
      pool.enqueue([this, &index, &mutex, &wg, &tree, &new_trees]() {
        try {
          read_tree(tree->hash(), tree);

          for (auto& inode : *tree) {
            std::lock_guard<std::mutex> lock(mutex);
            index.push_back(inode);

            if (inode->is_directory()) {
              new_trees.push_back(inode);
            }
          }

          wg.done();
        }
        catch (const std::exception& e) {
          wg.exception(e);
        }
      });
    }

    // Wait for all jobs to finish.
    wg.wait_rethrow();

    trees = std::move(new_trees);
  }
}

void cache::create_file(const std::filesystem::path& root, const inode::ptr& inode) {
  std::error_code ec;

  std::filesystem::path object_path = file_path(inode);

  if (!std::filesystem::create_directories(object_path.parent_path(), ec)) {
    // If the directory already exists, it's fine.
    if (ec) {
      throw std::runtime_error(
          "failed to create directory: " + object_path.parent_path().string() + ": " + ec.message());
    }
  }

  std::filesystem::copy_file(root / inode->path(), object_path, std::filesystem::copy_options::update_existing, ec);
  if (ec) {
    throw std::runtime_error("failed to copy file: " + inode->path() + ": " + ec.message());
  }

  std::filesystem::permissions(object_path, std::filesystem::perms(0600), ec);
  if (ec) {
    throw std::runtime_error("failed to set file permissions: " + inode->path() + ": " + ec.message());
  }
}

void cache::create_dirtree(inode::ptr& node) {
  std::error_code ec;

  node->sort();

  // Create ostream from file pointer
  std::ostringstream file(std::ios::binary);
  if (!file) {
    throw std::runtime_error(std::string("failed to create temporary file: ") + std::strerror(errno));
  }

  file << *node;
  file.flush();

  // Create temporary file using mkstemp, without knowing its hash yet, and write direntries.
  std::filesystem::path tmp = _tmpdir;
  FILE* fp = fstree::mkstemp(tmp);
  if (!fp) {
    throw std::runtime_error("failed to create temporary file: " + tmp.string() + ": " + std::strerror(errno));
  }
  if (fwrite(file.str().data(), 1, file.str().size(), fp) != file.str().size()) {
    int err = errno;
    fclose(fp);
    std::filesystem::remove(tmp, ec);
    throw std::runtime_error("failed to write to temporary file: " + tmp.string() + ": " + std::strerror(err));
  }
  fclose(fp);

  // Then calculate the hash of the file and move it to the object directory.
  fstree::digest hash = hashsum_hex_file(tmp);
  node->set_hash(hash);

  std::filesystem::path object_path = tree_path(node);
  if (std::filesystem::exists(object_path, ec)) {
    std::filesystem::remove(tmp, ec);
    return;
  }

  if (!std::filesystem::create_directories(object_path.parent_path(), ec)) {
    // If the directory already exists, it's fine.
    if (ec) {
      std::filesystem::remove(tmp, ec);
      throw std::runtime_error(
          "failed to create directory: " + object_path.parent_path().string() + ": " + ec.message());
    }
  }

  std::filesystem::rename(tmp, object_path, ec);
  if (ec) {
    std::filesystem::remove(tmp, ec);
    throw std::runtime_error("failed to rename temporary file: " + tmp.string() + ": " + ec.message());
  }
}

std::filesystem::path cache::file_path(const fstree::digest& hash) {
  auto hex = hash.hexdigest();
  return _objectdir / hex.substr(0, 2) / (hex.substr(2) + ".file");
}

std::filesystem::path cache::file_path(const inode::ptr& inode) { return file_path(inode->hash()); }

std::filesystem::path cache::tree_path(const fstree::digest& hash) {
  auto hex = hash.hexdigest();
  return _objectdir / hex.substr(0, 2) / (hex.substr(2) + ".tree");
}

std::filesystem::path cache::tree_path(const inode::ptr& inode) { return tree_path(inode->hash()); }

void cache::pull_object(fstree::remote& remote, const fstree::digest& hash) {
#ifdef _WIN32
  auto lock = _lock.lock();
#endif
  if (!has_object(hash)) {
    event("cache::pull_object", hash.string());
    std::filesystem::path object_path = file_path(hash);
    remote.read_object(hash, object_path, _tmpdir);
  }
}

void cache::pull_tree(fstree::remote& remote, const fstree::digest& hash) {
#ifdef _WIN32
  auto lock = _lock.lock();
#endif
  if (!has_tree(hash)) {
    event("cache::pull_tree", hash.string());
    std::filesystem::path object_path = tree_path(hash);
    remote.read_object(hash, object_path, _tmpdir);
  }
}

bool cache::has_object(const fstree::digest& hash) {
  std::filesystem::path object = file_path(hash);
  // Check if the object exists and is a regular file
  // by opening it for reading. This modifies the
  // access time so that the eviction algorithm can
  // take it into account.

  return fstree::touch(object);
}

bool cache::has_tree(const fstree::digest& hash) {
  std::filesystem::path object = tree_path(hash);
  // Check if the object exists and is a regular file
  // by opening it for reading. This modifies the
  // access time so that the eviction algorithm can
  // take it into account.

  return fstree::touch(object);
}

void cache::copy_file(const fstree::digest& hash, const std::filesystem::path& to) {
  std::filesystem::copy_file(file_path(hash), to, std::filesystem::copy_options::overwrite_existing);
}

void cache::push_object(fstree::remote& remote, const fstree::digest& hash) {
  event("cache::push_object", hash.string());
  std::filesystem::path object_path = file_path(hash);
  remote.write_object(hash, object_path);
}

void cache::push_tree(fstree::remote& remote, const fstree::digest& hash) {
  event("cache::push_tree", hash.string());
  std::filesystem::path object_path = tree_path(hash);
  remote.write_object(hash, object_path);
}

void cache::push(const fstree::index& index, fstree::remote& remote) {
  event("cache::push", index.root()->hash().string(), index.size());

  pool& pool = get_pool();
  wait_group wg;

  // List of trees to check for presence in the remote
  std::vector<fstree::digest> check_trees;

  // Mutex to protect the check_trees list when adding new trees
  std::mutex check_trees_mutex;

  // First add the root tree to be checked
  check_trees.push_back(index.root()->hash());

  do {
    // Lists of missing child trees and objects for the checked tree
    std::vector<fstree::digest> missing_trees, missing_objects;

    // Pop the last tree from the list to check
    fstree::digest tree_hash;
    {
      std::lock_guard<std::mutex> lock(check_trees_mutex);
      tree_hash = check_trees.back();
      check_trees.pop_back();
    }

    // Query the remote for missing trees and objects
    try {
      remote.has_tree(tree_hash, missing_trees, missing_objects);
    }
    catch (const fstree::unsupported_operation& e) {
      // If the has_tree method is unsupported, check all objects in the index manually

      // Check the root tree
      bool found = remote.has_object(tree_hash);
      if (!found) {
        missing_trees.push_back(tree_hash);
      }

      // Check all objects in the index
      for (const auto& inode : index) {
        bool found = remote.has_object(inode->hash());
        if (!found) {
          if (inode->is_directory()) {
            missing_trees.push_back(inode->hash());
          }
          else if (inode->is_file()) {
            missing_objects.push_back(inode->hash());
          }
        }
      }
    }

    // Write missing objects in parallel
    for (const auto& hash : missing_objects) {
      event("cache::remote_missing_object", hash.string());

      wg.add(1);
      pool.enqueue([this, &wg, &remote, hash]() {
        try {
          push_object(remote, hash);
          wg.done();
        }
        catch (const std::exception& e) {
          wg.exception(e);
        }
      });
    }

    // Write missing trees in parallel
    for (const auto& hash : missing_trees) {
      event("cache::remote_missing_tree", hash.string());

      wg.add(1);
      pool.enqueue([this, &check_trees, &check_trees_mutex, &wg, &remote, hash]() {
        try {
          push_tree(remote, hash);

          // Add the tree to the list of trees to check now that it's been pushed
          std::lock_guard<std::mutex> lock(check_trees_mutex);
          check_trees.push_back(hash);

          wg.done();
        }
        catch (const std::exception& e) {
          wg.exception(e);
        }
      });
    }

    {
      std::unique_lock<std::mutex> lock(check_trees_mutex);
      if (!check_trees.empty()) {
        continue;
      }
    }

    wg.wait_rethrow();
  } while (!check_trees.empty());
}

void cache::pull(fstree::index& index, fstree::remote& remote, const fstree::digest& tree_hash) {
  event("cache::pull", tree_hash.string(), index.size());

  pool& pool = get_pool();
  wait_group wg;

  // List of missing objects
  std::vector<inode::ptr> trees;

  // Add root directory
  index.root()->set_hash(tree_hash);
  index.root()->set_status(fstree::file_status(fs::file_type::directory, fs::perms::none));
  trees.push_back(index.root());

  // Check if trees are present locally, otherwise pull the tree from the remote.
  // Pull all objects in parallel.
  while (!trees.empty()) {
    // Pull all discovered trees in parallel
    for (auto& tree : trees) {
      wg.add(1);
      pool.enqueue([this, &wg, &remote, &tree]() {
        try {
          pull_tree(remote, tree->hash());
          read_tree(tree->hash(), tree);
          wg.done();
        }
        catch (const std::exception& e) {
          wg.exception(e);
        }
      });
    }

    // Wait for all jobs to finish
    wg.wait_rethrow();

    // Pull all objects in parallel, add trees to list of trees to pull
    std::mutex mutex;
    std::vector<inode::ptr> new_trees;
    for (auto& tree : trees) {
      wg.add(1);
      pool.enqueue([this, &index, &wg, &remote, tree, &new_trees, &pool, &mutex]() {
        for (auto& inode : *tree) {
          {
            std::lock_guard<std::mutex> lock(mutex);
            index.push_back(inode);
          }

          if (inode->is_symlink()) continue;

          if (inode->is_directory()) {
            std::lock_guard<std::mutex> lock(mutex);
            new_trees.push_back(inode);
            continue;
          }

          wg.add(1);
          pool.enqueue([this, &wg, &remote, inode]() {
            try {
              pull_object(remote, inode->hash());
              wg.done();
            }
            catch (const std::exception& e) {
              wg.exception(e);
            }
          });
        }

        wg.done();
      });
    }

    // Wait for all jobs to finish
    wg.wait_rethrow();

    trees = std::move(new_trees);
  }
}

void cache::evict() {
  fstree::wait_group wg;

  for (const auto& entry : sorted_directory_iterator(_objectdir, glob_list(), false)) {
    if (entry->is_directory()) {
      wg.add(1);
      get_pool().enqueue([this, entry, &wg]() {
        try {
          evict_subdir(_objectdir / entry->path());
          wg.done();
        }
        catch (const std::exception& e) {
          wg.exception(e);
        }
      });
    }
  }

  wg.wait_rethrow();
}

void cache::evict_subdir(const std::filesystem::path& dir) {
  const auto sort_by_mtime = [](const inode::ptr& a, const inode::ptr& b) {
    return a->last_write_time() < b->last_write_time();
  };
  sorted_directory_iterator objects(dir, glob_list(), sort_by_mtime, false);

  // Summarize the size of all objects in the directory
  size_t size = 0;
  for (const auto& inode : objects) {
    size += inode->size();
  }

  // Evict objects until the size is below the cache limit
  for (const auto& inode : objects) {
    if (size < _max_size_slice) {
      break;
    }

    auto lock = _lock.lock();

    // First check if the object is still present and if it is, check the access time
    // with the cache lock held.
    fstree::stat status;

    try {
      fstree::lstat(dir / inode->path(), status);
    }
    catch (const std::exception& e) {
      // The object has likely been removed by another thread, continue with the next object.
      continue;
    }

    // Skip objects that have been accessed recently.
    auto mtime = std::chrono::nanoseconds(status.last_write_time);
    auto curtime = std::chrono::system_clock::now().time_since_epoch();
    if (mtime + _retention_period > curtime) {
      continue;
    }

    std::error_code ec;
    std::filesystem::remove(dir / inode->path(), ec);
    if (ec) {
      throw std::runtime_error("failed to remove cache object: " + inode->hash().string() + ": " + ec.message());
    }

    size -= inode->size();

    event("cache::evict", (dir / inode->path()).string());
  }
}

}  // namespace fstree
