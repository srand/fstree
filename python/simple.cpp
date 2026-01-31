#include "simple.hpp"

#include "cache.hpp"
#include "index.hpp"
#include "remote.hpp"
#include "url.hpp"

namespace fstree {

    
std::string simple::write_tree(const std::string& path) {
    index index(path);
    try {
      index.load();
    }
    catch (const std::exception& e) {
    }

    index.refresh();
    _cache.add(index);
    index.save();

    return index.root()->hash();
}

std::string simple::write_tree_push(const std::string& path, const std::string& remote_url) {
    index index(path);
    try {
      index.load();
    }
    catch (const std::exception& e) {
    }

    index.refresh();
    _cache.add(index);

    std::unique_ptr<fstree::remote> remote = fstree::remote::create(url(remote_url));
    _cache.push(index, *remote);

    index.save();

    return index.root()->hash();
}

void simple::push(const std::string& tree_hash, const std::string& remote_url) {
    std::unique_ptr<fstree::remote> remote = fstree::remote::create(url(remote_url));
    fstree::index index;
    _cache.index_from_tree(tree_hash, index);
    _cache.push(index, *remote);
}

void simple::pull(const std::string& tree_hash, const std::string& remote_url) {
    std::unique_ptr<fstree::remote> remote = fstree::remote::create(url(remote_url));
    fstree::index index;
    _cache.pull(index, *remote, tree_hash);
}

void simple::pull_checkout(const std::string& tree_hash, const std::string& remote_url, const std::string& dest_path) {
    std::unique_ptr<fstree::remote> remote = fstree::remote::create(url(remote_url));
    fstree::index index;
    _cache.pull(index, *remote, tree_hash);
    index.checkout(_cache, std::filesystem::path(dest_path));
    index.save();
}

void simple::checkout(const std::string& tree_hash, const std::string& dest_path) {
    fstree::index index;
    _cache.index_from_tree(tree_hash, index);
    index.checkout(_cache, std::filesystem::path(dest_path));
    index.save();
}

}