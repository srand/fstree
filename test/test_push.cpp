#include "cache.hpp"
#include "exception.hpp"
#include "glob_list.hpp"
#include "index.hpp"
#include "inode.hpp"
#include "remote.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

using namespace fstree;
namespace fs = std::filesystem;

namespace {

// In-memory remote that answers lookups the way the jolt cache service does,
// except that it reports every occurrence of a missing tree or object rather
// than each digest once. Clients must not rely on the remote to deduplicate.
class fake_remote : public fstree::remote {
 public:
  bool has_object(const fstree::digest& hash) override {
    std::lock_guard<std::mutex> lock(_mutex);
    return _objects.count(hash.string()) > 0;
  }

  void has_objects(const std::vector<fstree::digest>& hashes, std::vector<bool>& presence) override {
    for (const auto& hash : hashes) {
      presence.push_back(has_object(hash));
    }
  }

  void has_tree(const std::vector<fstree::digest>& hashes, const missing_callback& callback) override {
    {
      std::lock_guard<std::mutex> lock(_mutex);
      _has_tree_calls++;
    }

    // Report the trees one at a time, as the service streams its responses.
    for (const auto& hash : hashes) {
      std::vector<fstree::digest> missing_trees, missing_objects;
      check_tree(hash, missing_trees, missing_objects);
      if (missing_trees.empty() && missing_objects.empty()) {
        continue;
      }

      std::unique_lock<std::mutex> lock(_mutex);
      int writes = _write_count;
      lock.unlock();

      callback(missing_trees, missing_objects);

      // Uploads should begin while the rest of the response is still
      // being received. Give them some time to show up.
      lock.lock();
      if (!_cv.wait_for(lock, std::chrono::seconds(2), [&] { return _write_count > writes; })) {
        _uploads_waited_for_lookup = true;
      }
    }
  }

  void write_object(const fstree::digest& hash, const std::filesystem::path& path) override {
    std::ifstream file(path, std::ios::binary);
    std::stringstream content;
    content << file.rdbuf();

    std::lock_guard<std::mutex> lock(_mutex);
    _objects[hash.string()] = content.str();
    _writes[hash.string()]++;
    _write_count++;
    _cv.notify_all();
  }

  void read_object(const fstree::digest&, const std::filesystem::path&, const std::filesystem::path&) override {
    throw fstree::unsupported_operation("fake_remote::read_object is not implemented");
  }

  // Number of times each object was written.
  std::map<std::string, int> writes() {
    std::lock_guard<std::mutex> lock(_mutex);
    return _writes;
  }

  // Returns true if uploads did not start until a lookup had finished.
  bool uploads_waited_for_lookup() {
    std::lock_guard<std::mutex> lock(_mutex);
    return _uploads_waited_for_lookup;
  }

  int has_tree_calls() {
    std::lock_guard<std::mutex> lock(_mutex);
    return _has_tree_calls;
  }

  // Returns true if the tree and everything below it is present.
  bool is_complete(const fstree::digest& hash) {
    std::vector<fstree::digest> missing_trees, missing_objects;
    check_tree(hash, missing_trees, missing_objects);
    return missing_trees.empty() && missing_objects.empty();
  }

 protected:
  void check_tree(
      const fstree::digest& hash,
      std::vector<fstree::digest>& missing_trees,
      std::vector<fstree::digest>& missing_objects) {
    std::string content;
    {
      std::lock_guard<std::mutex> lock(_mutex);
      auto it = _objects.find(hash.string());
      if (it == _objects.end()) {
        missing_trees.push_back(hash);
        return;
      }
      content = it->second;
    }

    std::istringstream stream(content);
    inode::ptr tree = fstree::make_intrusive<fstree::inode>();
    stream >> *tree;

    for (const auto& child : *tree) {
      if (child->is_directory()) {
        check_tree(child->hash(), missing_trees, missing_objects);
      }
      else if (child->is_file() && !has_object(child->hash())) {
        missing_objects.push_back(child->hash());
      }
    }

    tree->clear();
  }

  std::mutex _mutex;
  std::condition_variable _cv;
  std::map<std::string, std::string> _objects;
  std::map<std::string, int> _writes;
  int _has_tree_calls = 0;
  int _write_count = 0;
  bool _uploads_waited_for_lookup = false;
};

class PushTest : public ::testing::Test {
 protected:
  void SetUp() override {
    root = fs::temp_directory_path() / "fstree_test_push";
    fs::remove_all(root);
    fs::create_directories(workspace());
  }

  void TearDown() override { fs::remove_all(root); }

  fs::path workspace() const { return root / "workspace"; }

  void CreateFile(const std::string& rel, const std::string& content) {
    fs::path full = workspace() / rel;
    fs::create_directories(full.parent_path());
    std::ofstream(full, std::ios::binary) << content;
  }

  // Index the workspace, add it to a local cache and push it to the remote.
  // Returns the digest of the workspace tree.
  fstree::digest Push(fstree::remote& remote) {
    fstree::cache cache(root / "cache", fstree::cache::default_max_size, fstree::cache::default_retention);
    fstree::index index(workspace(), fstree::glob_list());
    index.refresh();
    cache.add(index);
    cache.push(index, remote);
    return index.root()->hash();
  }

  fs::path root;
};

}  // namespace

TEST_F(PushTest, PushesCompleteTree) {
  CreateFile("README.md", "readme");
  CreateFile("src/main.cpp", "main");
  CreateFile("src/lib/lib.cpp", "lib");

  fake_remote remote;
  fstree::digest tree = Push(remote);

  EXPECT_TRUE(remote.is_complete(tree));
}

TEST_F(PushTest, UploadsEachDigestOnce) {
  // Identical directories and files share digests, and the remote reports
  // every occurrence as missing.
  CreateFile("a/same.txt", "same");
  CreateFile("b/same.txt", "same");
  CreateFile("c/other.txt", "same");
  CreateFile("c/d/same.txt", "same");

  fake_remote remote;
  fstree::digest tree = Push(remote);

  EXPECT_TRUE(remote.is_complete(tree));
  for (const auto& [digest, count] : remote.writes()) {
    EXPECT_EQ(1, count) << digest << " uploaded more than once";
  }
}

TEST_F(PushTest, LooksUpTreesOneLevelAtATime) {
  // Directories pushed in the same round must be looked up together, so
  // the number of lookups follows the depth of the tree and not its width.
  for (int i = 0; i < 5; i++) {
    CreateFile("dir" + std::to_string(i) + "/file.txt", "file " + std::to_string(i));
  }

  fake_remote remote;
  fstree::digest tree = Push(remote);

  EXPECT_TRUE(remote.is_complete(tree));

  // The root is found missing, then its children, then theirs.
  EXPECT_EQ(3, remote.has_tree_calls());
}

TEST_F(PushTest, UploadsWhileLookingUp) {
  // Missing trees and objects are reported while the lookup is still in
  // progress, and uploading them should not wait for it to complete.
  for (int i = 0; i < 3; i++) {
    CreateFile("dir" + std::to_string(i) + "/file.txt", "file " + std::to_string(i));
  }

  fake_remote remote;
  fstree::digest tree = Push(remote);

  EXPECT_TRUE(remote.is_complete(tree));
  EXPECT_FALSE(remote.uploads_waited_for_lookup());
}
