#include "simple.hpp"

#include "cache.hpp"
#include "index.hpp"
#include "remote.hpp"
#include "url.hpp"

namespace fstree {

void simple::checkout(const std::string& dest_path) {
    _index.checkout(_cache, std::filesystem::path(dest_path));
}

void simple::load(const std::string& tree_hash) {
    fstree::index index;
    _cache.index_from_tree(fstree::digest::parse(tree_hash), index);
    _index = std::move(index);
}

std::optional<inode::ptr> simple::lookup(const std::string& path) const {
    auto ptr = _index.find_node_by_path(std::filesystem::path(path));
    return ptr ? std::optional<inode::ptr>(ptr) : std::nullopt;
}

void simple::pull(const std::string& tree_hash, const std::string& remote_url) {
    std::unique_ptr<fstree::remote> remote = fstree::remote::create(url(remote_url));
    fstree::index index;
    _cache.pull(index, *remote, fstree::digest::parse(tree_hash));
    _index = std::move(index);
}

void simple::push(const std::string& remote_url) {
    std::unique_ptr<fstree::remote> remote = fstree::remote::create(url(remote_url));
    _cache.push(_index, *remote);
}

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

std::string simple::write_tree(const std::vector<std::string>& paths) {
    fstree::index index;

    for (const auto& path : paths) {
        fstree::index other_index(path);
        try {
            other_index.load();
        }
        catch (const std::exception& e) {
        }
        other_index.refresh();
        _cache.add(other_index);
        other_index.save();
        index.merge(other_index);
    }


    _cache.add(index);
    index.save();

    _index = std::move(index);
    return _index.root()->hash().string();
}

} // namespace fstree
