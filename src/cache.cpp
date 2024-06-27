#include "cache.hpp"

#include "filesystem.hpp"
#include "inode.hpp"
#include "log.hpp"
#include "thread.hpp"
#include "thread_pool.hpp"
#include "wait_group.hpp"

#include <cstring>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

namespace fstree {

cache::cache(const std::filesystem::path& path) : _objectdir(path / "objects"), _tmpdir(path / "tmp") {
  std::error_code ec;

  if (!std::filesystem::exists(_objectdir, ec)) {
    if (!std::filesystem::create_directories(_objectdir, ec)) {
      throw std::runtime_error("failed to create cache object directory: " + _objectdir.string() + ": " + ec.message());
    }
  }

  if (!std::filesystem::exists(_tmpdir, ec)) {
    if (!std::filesystem::create_directories(_tmpdir, ec)) {
      throw std::runtime_error("failed to create cache temporary directory: " + _tmpdir.string() + ": " + ec.message());
    }
  }
}

void cache::add(fstree::index& index) {
  pool& pool = get_pool();
  wait_group wg;

  // List of dirty directory inodes
  std::vector<inode*> dirty_dirs;

  for (const auto& inode : index) {
    if (inode->is_file()) {
      wg.add(1);
      pool.enqueue([this, &index, inode, &wg]() {
        try {
          std::error_code ec;

          if (inode->is_dirty()) {
            log(log_level::debug) << "adding object: " << inode->path() << std::endl;
            inode->rehash(index.root_path());
          }

          create_file(index.root_path(), inode);

          wg.done();
        }
        catch (const std::exception& e) {
          wg.exception(e);
        }
      });
    }
    else if (inode->is_directory() && inode->is_dirty()) {
      dirty_dirs.push_back(inode);
    }
  }

  wg.wait_rethrow();

  // Create directory nodes in reverse order
  for (auto it = dirty_dirs.rbegin(); it != dirty_dirs.rend(); ++it) {
    create_dirtree(*it);
  }

  create_dirtree(&index.root());
}

void cache::read_tree(const std::string& hash, inode& inode) {
  std::error_code ec;

  inode.set_hash(hash);

  std::filesystem::path object = tree_path(&inode);
  if (!std::filesystem::exists(object, ec)) {
    throw std::runtime_error("tree object not found in local cache: " + hash);
  }

  std::ifstream file(object, std::ios::binary);
  if (!file) {
    throw std::runtime_error("failed to open tree object: " + object.string() + ": " + ec.message());
  }

  file >> inode;
}

void cache::index_from_tree(const std::string& hash, fstree::index& index) {
  std::vector<inode*> trees;
  pool& pool = get_pool();
  std::mutex mutex;
  wait_group wg;

  // Add root directory
  index.root().set_hash(hash);
  index.root().set_status(fstree::file_status(fs::file_type::directory, fs::perms::none));
  trees.push_back(&index.root());

  // Check if trees are present locally, otherwise pull the tree from the remote.
  // Pull all objects in parallel.
  while (!trees.empty()) {
    std::vector<inode*> new_trees;

    // Pull all discovered trees in parallel
    for (auto& tree : trees) {
      wg.add(1);
      pool.enqueue([this, &index, &mutex, &wg, tree, &new_trees]() {
        try {
          read_tree(tree->hash(), *tree);

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

void cache::create_file(const std::filesystem::path& root, const inode* inode) {
  std::error_code ec;

  std::filesystem::path object_path = file_path(inode);
  if (std::filesystem::exists(object_path, ec)) {
    return;
  }

  if (!std::filesystem::create_directories(object_path.parent_path(), ec)) {
    // If the directory already exists, it's fine.
    if (ec && ec.value() != EEXIST) {
      throw std::runtime_error(
          "failed to create directory: " + object_path.parent_path().string() + ": " + ec.message());
    }
  }

  std::filesystem::copy_file(root / inode->path(), object_path, std::filesystem::copy_options::update_existing, ec);
  if (ec) {
    throw std::runtime_error("failed to copy file: " + inode->path() + ": " + ec.message());
  }
}

void cache::create_dirtree(inode* node) {
  std::error_code ec;

  node->sort();

  // Create ostream from file pointer
  std::ostringstream file;
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
    fclose(fp);
    throw std::runtime_error("failed to write to temporary file: " + tmp.string() + ": " + std::strerror(errno));
  }
  fclose(fp);

  // Then calculate the hash of the file and move it to the object directory.
  std::string hash = sha1_hex_file(tmp);
  node->set_hash(hash);

  std::filesystem::path object_path = tree_path(node);
  if (std::filesystem::exists(object_path, ec)) {
    return;
  }

  if (!std::filesystem::create_directories(object_path.parent_path(), ec)) {
    // If the directory already exists, it's fine.
    if (ec && ec.value() != EEXIST) {
      throw std::runtime_error(
          "failed to create directory: " + object_path.parent_path().string() + ": " + ec.message());
    }
  }

  std::filesystem::rename(tmp, object_path, ec);
  if (ec) {
    throw std::runtime_error("failed to rename temporary file: " + tmp.string() + ": " + ec.message());
  }
}

std::filesystem::path cache::file_path(const std::string& hash) {
  return _objectdir / hash.substr(0, 2) / (hash.substr(2) + ".file");
}

std::filesystem::path cache::file_path(const inode* inode) { return file_path(inode->hash()); }

std::filesystem::path cache::tree_path(const std::string& hash) {
  return _objectdir / hash.substr(0, 2) / (hash.substr(2) + ".tree");
}

std::filesystem::path cache::tree_path(const inode* inode) { return tree_path(inode->hash()); }

void cache::pull_object(fstree::remote& remote, const std::string& hash) {
  if (!has_object(hash)) {
    log(log_level::debug) << "pulling object: " << hash << std::endl;
    std::filesystem::path object_path = file_path(hash);
    remote.read_object(hash, object_path, _tmpdir);
  }
}

void cache::pull_tree(fstree::remote& remote, const std::string& hash) {
  if (!has_tree(hash)) {
    std::filesystem::path object_path = tree_path(hash);
    remote.read_object(hash, object_path, _tmpdir);
  }
}

bool cache::has_object(const std::string& hash) {
  std::filesystem::path object = file_path(hash);
  return std::filesystem::exists(object);
}

bool cache::has_tree(const std::string& hash) {
  std::filesystem::path object = tree_path(hash);
  return std::filesystem::exists(object);
}

void cache::copy(const std::string& hash, const std::filesystem::path& to) {
  std::error_code ec;
  std::filesystem::copy(file_path(hash), to, std::filesystem::copy_options::overwrite_existing, ec);
  if (ec) {
    throw std::runtime_error("failed to copy object: " + hash + ": " + ec.message());
  }
}

void cache::push_object(fstree::remote& remote, const std::string& hash) {
  log(log_level::debug) << "pushing object: " << hash << std::endl;
  std::filesystem::path object_path = file_path(hash);
  remote.write_object(hash, object_path);
}

void cache::push_tree(fstree::remote& remote, const std::string& hash) {
  std::filesystem::path object_path = tree_path(hash);
  remote.write_object(hash, object_path);
}

void cache::push(const fstree::index& index, fstree::remote& remote) {
  pool& pool = get_pool();
  wait_group wg;

  // List of trees to check for presence in the remote
  std::vector<std::string> check_trees;

  // Mutex to protect the check_trees list when adding new trees
  std::mutex check_trees_mutex;

  // First add the root tree to be checked
  check_trees.push_back(index.root().hash());

  do {
    // Lists of missing child trees and objects for the checked tree
    std::vector<std::string> missing_trees, missing_objects;

    // Pop the last tree from the list to check
    std::string tree_hash = check_trees.back();
    check_trees.pop_back();

    // Query the remote for missing trees and objects
    remote.has_tree(tree_hash, missing_trees, missing_objects);

    // Write missing objects in parallel
    for (const auto& hash : missing_objects) {
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

    wg.wait_rethrow();

  } while (!check_trees.empty());
}

void cache::pull(fstree::index& index, fstree::remote& remote, const std::string& tree_hash) {
  pool& pool = get_pool();
  wait_group wg;

  // List of missing objects
  std::vector<inode*> trees;

  // Add root directory
  index.root().set_hash(tree_hash);
  index.root().set_status(fstree::file_status(fs::file_type::directory, fs::perms::none));
  trees.push_back(&index.root());

  // Check if trees are present locally, otherwise pull the tree from the remote.
  // Pull all objects in parallel.
  while (!trees.empty()) {
    // Pull all discovered trees in parallel
    for (auto& tree : trees) {
      wg.add(1);
      pool.enqueue([this, &wg, &remote, &tree]() {
        try {
          pull_tree(remote, tree->hash());
          read_tree(tree->hash(), *tree);
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
    std::vector<inode*> new_trees;
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

}  // namespace fstree
