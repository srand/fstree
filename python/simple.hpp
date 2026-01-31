#pragma once 

#include "url.hpp"
#include "cache.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace fstree {


class simple {
public:
    simple() = default;

    std::string write_tree(const std::string& paths);
    std::string write_tree_push(const std::string& paths, const std::string& remote_url);
    void push(const std::string& tree_hash, const std::string& remote_url);
    void pull(const std::string& tree_hash, const std::string& remote_url);
    void pull_checkout(const std::string& tree_hash, const std::string& remote_url, const std::string& dest_path);
    void checkout(const std::string& tree_hash, const std::string& dest_path);

private:
    cache _cache;
};

} // namespace fstree
