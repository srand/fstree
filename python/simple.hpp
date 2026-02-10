#pragma once 

#include "cache.hpp"
#include "index.hpp"
#include "url.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace fstree {


class simple {
public:
    simple() = default;

    auto begin() const { return _index.begin(); }
    auto end() const { return _index.end(); }

    void checkout(const std::string& dest_path);
    void load(const std::string& tree_hash);
    std::optional<fstree::inode::ptr> lookup(const std::string& path) const;
    void push(const std::string& remote_url);
    void pull(const std::string& tree_hash, const std::string& remote_url);
    std::string write_tree(const std::string& path);
    std::string write_tree(const std::vector<std::string>& paths);

private:
    cache _cache;
    index _index;
};

} // namespace fstree
