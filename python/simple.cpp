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

    _index = std::move(index);

    return _index.root()->hash().string();
}

std::string simple::write_tree_push(const std::string& path, const std::string& remote_url) {
    write_tree(path);

    std::unique_ptr<fstree::remote> remote = fstree::remote::create(url(remote_url));
    _cache.push(_index, *remote);

    return _index.root()->hash().string();
}

void simple::push(const std::string& tree_hash, const std::string& remote_url) {
    std::unique_ptr<fstree::remote> remote = fstree::remote::create(url(remote_url));
    fstree::index index;
    _cache.index_from_tree(fstree::digest::parse(tree_hash), index);
    _cache.push(index, *remote);
}

void simple::pull(const std::string& tree_hash, const std::string& remote_url) {
    std::unique_ptr<fstree::remote> remote = fstree::remote::create(url(remote_url));
    fstree::index index;
    _cache.pull(index, *remote, fstree::digest::parse(tree_hash));
    _index = std::move(index);
}

void simple::pull_checkout(const std::string& tree_hash, const std::string& remote_url, const std::string& dest_path) {
    pull(tree_hash, remote_url);
    _index.checkout(_cache, std::filesystem::path(dest_path));
    _index.save();
}

void simple::checkout(const std::string& tree_hash, const std::string& dest_path) {
    fstree::index index;
    _cache.index_from_tree(fstree::digest::parse(tree_hash), index);
    index.checkout(_cache, std::filesystem::path(dest_path));
    index.save();
    _index = std::move(index);
}

bool simple::lookup(const std::string& path, std::string& _hash) {
    fstree::digest hash;
    bool found = _index.lookup(path, hash);
    if (found) {
        _hash = hash.string();
    }
    return found;
}

}