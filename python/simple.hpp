#pragma once 

#include "cache.hpp"
#include "index.hpp"
#include "url.hpp"

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace fstree {


class simple {
public:
    simple() = default;
    simple(const std::filesystem::path& cache_dir)
        : _cache(cache_dir, cache::default_max_size, cache::default_retention) {}
    simple(const std::filesystem::path& cache_dir, size_t max_cache_size, std::chrono::seconds retention_period)
        : _cache(cache_dir, max_cache_size, retention_period) {}

    const std::filesystem::path& indexfile() const { return _indexfile; }
    const std::filesystem::path& ignorefile() const { return _ignorefile; }
    void set_indexfile(const std::filesystem::path& indexfile) { _indexfile = indexfile; }
    void set_ignorefile(const std::filesystem::path& ignorefile) { _ignorefile = ignorefile; }

    auto begin() const { return _index.begin(); }
    auto end() const { return _index.end(); }

    std::string checkout(const std::string& dest_path);
    std::vector<fstree::inode::ptr> glob(const std::string& pattern) const;
    std::vector<fstree::inode::ptr> glob(const fstree::glob_list& patterns) const;
    void load(const std::string& tree_hash);
    std::optional<fstree::inode::ptr> lookup(const std::string& path) const;
    void push(const std::string& remote_url);
    void pull(const std::string& tree_hash, const std::string& remote_url);
    std::string write_tree(const std::string& path);
    std::string write_tree(const std::vector<std::string>& paths);

private:
    cache _cache;
    index _index;
    std::filesystem::path _indexfile = ".fstree/index";
    std::filesystem::path _ignorefile = ".fstreeignore";
};

} // namespace fstree
