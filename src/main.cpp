#include "argparser.hpp"
#include "cache.hpp"
#include "event.hpp"
#include "filesystem.hpp"
#include "ignore.hpp"
#include "index.hpp"
#include "remote_jolt.hpp"
#include "sha1.hpp"
#include "thread.hpp"
#include "url.hpp"
#include "version.hpp"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

int usage() {
  std::cerr << "fstree ls-index [<directory>]" << std::endl;
  std::cerr << "fstree ls-tree [--cache <dir>] <tree>" << std::endl;
  std::cerr << "fstree pull [--cache <dir>] [--remote <url>] [--threads <int>] <tree>" << std::endl;
  std::cerr << "fstree pull-checkout [--cache <dir>] [--remote <url>] [--threads <int>] <tree> [<directory>]"
            << std::endl;
  std::cerr << "fstree push [--cache <dir>] [--remote <url>] [--threads <int>] [<directory>]" << std::endl;
  std::cerr << "fstree write-tree [--cache <dir>] [--ignore <conf>] [--threads <int>] [<directory>]" << std::endl;
  std::cerr
      << "fstree write-tree-push [--cache <dir>] [--ignore <conf>] [--remote <url>] [--threads <int>] [<directory>]"
      << std::endl;
  return EXIT_FAILURE;
}

int version() {
  std::cout << "fstree " << FSTREE_VERSION << std::endl;
  return EXIT_SUCCESS;
}

std::string tolower(std::string s) {
  // Convert string to lowercase
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
  return s;
}

int error(const std::string& msg) {
  std::cerr << "error: " << tolower(msg) << std::endl;
  return EXIT_FAILURE;
}

int error(const std::string& msg, const std::error_code& ec) {
  std::cerr << "error: " << tolower(msg) << ": " << tolower(ec.message()) << std::endl;
  return EXIT_FAILURE;
}

std::string rfc3339(std::chrono::nanoseconds since_epoch) {
  // Convert time to RFC3339 format
  auto tp = std::chrono::system_clock::time_point(std::chrono::duration_cast<std::chrono::seconds>(since_epoch));
  auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(since_epoch - tp.time_since_epoch());
  auto c_now = std::chrono::system_clock::to_time_t(tp);

  std::stringstream ss;
  ss << std::put_time(gmtime(&c_now), "%FT%T") << '.' << std::setfill('0') << std::setw(3) << millis.count() << 'Z';
  return ss.str();
}

std::filesystem::path current_path() { return std::filesystem::current_path(); }

int cmd_fstree(const fstree::argparser& args) {
  fstree::url remoteurl(args.get_option("--remote"));
  if (remoteurl.host().empty()) throw std::invalid_argument("invalid remote URL: " + args.get_option("--remote"));

  std::filesystem::path cachedir = args.get_option_path("--cache");
  if (cachedir.empty()) throw std::invalid_argument("unknown cache directory");

  std::filesystem::path ignorefile = args.get_option_path("--ignore", false);
  if (ignorefile.empty()) throw std::invalid_argument("unknown ignore directory");

  std::filesystem::path indexfile = args.get_option_path("--index", false);
  if (indexfile.empty()) throw std::invalid_argument("unknown index path");

  std::string threads = args.get_option("--threads");
  if (threads.empty()) throw std::invalid_argument("unknown thread count");
  try {
    fstree::set_hardware_concurrency(std::stoi(threads));
  }
  catch (const std::exception& e) {
    throw std::invalid_argument("invalid thread count: " + threads);
  }

  size_t cachesize = 0;
  try {
    cachesize = fstree::parse_size(args.get_option("--cache-size"));
  }
  catch (const std::exception& e) {
    throw std::invalid_argument("invalid cache size: " + args.get_option("--cache-size"));
  }

  std::chrono::seconds retention_period;
  try {
    retention_period = std::chrono::seconds(std::stoi(args.get_option("--cache-retention")));
  }
  catch (const std::exception& e) {
    throw std::invalid_argument("invalid cache retention period: " + args.get_option("--cache-retention"));
  }

  if (args.size() < 1) throw std::invalid_argument("missing command argument");

  fstree::cache cache(cachedir, cachesize, retention_period);

  if (args[0] == "checkout") {
    if (args.size() < 2) throw std::invalid_argument("missing tree argument");

    std::string tree = args[1];
    if (tree.empty()) throw std::invalid_argument("missing tree argument");

    std::filesystem::path workspace = args.size() > 2 ? args.get_value_path(2) : current_path();
    if (workspace.empty()) throw std::invalid_argument("missing workspace argument");

    fstree::index lindex(workspace);
    fstree::index rindex(workspace);

    try {
      lindex.load(indexfile);
    }
    catch (const std::exception& e) {
      if (fstree::events_enabled())
        fstree::event("warning", indexfile.string(), "failed to load index: " + tolower(e.what()));
      else
        std::cerr << "warning: failed to load index: " << tolower(e.what()) << std::endl;
    }

    cache.index_from_tree(tree, rindex);
    rindex.sort();
    rindex.copy_metadata(lindex);
    rindex.load_ignore_from_index(cache, ignorefile);
    rindex.checkout(cache, workspace);
    rindex.save(indexfile);

    std::cout << rindex.root().hash() << std::endl;
    return EXIT_SUCCESS;
  }
  else if (args[0] == "ls-index") {
    if (args.size() < 1) return usage();
    std::filesystem::path workspace = args.size() > 1 ? args.get_value_path(1) : current_path();
    fstree::index index(workspace);

    index.load(indexfile);

    for (const auto& inode : index) {
      auto mtime = std::chrono::nanoseconds(inode->last_write_time());
      if (inode->is_symlink())
        std::cout << std::setw(40) << inode->hash() << " " << inode->status().str() << " " << rfc3339(mtime) << " "
                  << inode->path() << " -> " << inode->target() << std::endl;
      else
        std::cout << std::setw(40) << inode->hash() << " " << inode->status().str() << " " << rfc3339(mtime) << " "
                  << inode->path() << std::endl;
    }

    return EXIT_SUCCESS;
  }
  else if (args[0] == "ls-tree") {
    if (args.size() < 2) throw std::invalid_argument("missing tree argument");
    std::string tree = args[1];
    if (tree.empty()) throw std::invalid_argument("missing tree argument");

    fstree::inode root;
    cache.read_tree(tree, root);

    for (const auto& inode : root) {
      if (inode->is_symlink())
        std::cout << std::setw(40) << inode->hash() << " " << inode->status().str() << " " << inode->path() << " -> "
                  << inode->target() << std::endl;
      else
        std::cout << std::setw(40) << inode->hash() << " " << inode->status().str() << " " << inode->path()
                  << std::endl;
    }

    return EXIT_SUCCESS;
  }
  else if (args[0] == "pull") {
    // Pulls a tree from a remote cache

    if (args.size() < 2) throw std::invalid_argument("missing tree argument");

    std::string tree = args[1];
    if (tree.empty()) throw std::invalid_argument("missing tree argument");

    std::unique_ptr<fstree::remote> remote = fstree::remote::create(remoteurl);
    fstree::index index;

    cache.pull(index, *remote, tree);
    cache.evict();

    std::cout << index.root().hash() << std::endl;
    return EXIT_SUCCESS;
  }
  else if (args[0] == "pull-checkout") {
    // Pulls a tree from a remote cache and checks it out
    if (args.size() < 2) throw std::invalid_argument("missing tree argument");

    std::string tree = args[1];
    if (tree.empty()) throw std::invalid_argument("missing tree argument");

    std::filesystem::path workspace = args.size() > 2 ? args.get_value_path(2) : current_path();
    if (workspace.empty()) throw std::invalid_argument("missing workspace argument");

    std::unique_ptr<fstree::remote> remote = fstree::remote::create(remoteurl);
    fstree::index rindex(workspace), lindex(workspace);

    try {
      lindex.load(indexfile);
    }
    catch (const std::exception& e) {
      if (fstree::events_enabled())
        fstree::event("warning", indexfile.string(), "failed to load index: " + tolower(e.what()));
      else
        std::cerr << "warning: failed to load index: " << tolower(e.what()) << std::endl;
    }

    cache.pull(rindex, *remote, tree);
    cache.evict();
    rindex.sort();
    rindex.copy_metadata(lindex);
    rindex.load_ignore_from_index(cache, ignorefile);
    rindex.checkout(cache, workspace);
    rindex.save(indexfile);

    std::cout << rindex.root().hash() << std::endl;
    return EXIT_SUCCESS;
  }
  else if (args[0] == "push") {
    if (args.size() < 2) throw std::invalid_argument("missing tree argument");

    std::string tree = args[1];
    if (tree.empty()) throw std::invalid_argument("missing tree argument");

    std::unique_ptr<fstree::remote> remote = fstree::remote::create(remoteurl);
    fstree::index index;

    cache.index_from_tree(tree, index);
    cache.push(index, *remote);

    std::cout << index.root().hash() << std::endl;
    return EXIT_SUCCESS;
  }
  else if (args[0] == "write-tree") {
    std::filesystem::path workspace = args.size() > 1 ? args.get_value_path(1) : current_path();
    if (workspace.empty()) throw std::invalid_argument("missing workspace argument");

    fstree::ignore_list ignores;
    try {
      ignores.load((workspace / ignorefile).string());
    }
    catch (const std::exception& e) {
    }

    fstree::index index(workspace, ignores);
    try {
      index.load(indexfile);
    }
    catch (const std::exception& e) {
      if (fstree::events_enabled())
        fstree::event("warning", indexfile.string(), "failed to load index: " + tolower(e.what()));
      else
        std::cerr << "warning: failed to load index: " << tolower(e.what()) << std::endl;
    }

    index.refresh();
    cache.add(index);
    cache.evict();
    index.save(indexfile);

    std::cout << index.root().hash() << std::endl;
    return EXIT_SUCCESS;
  }
  else if (args[0] == "write-tree-push") {
    // Pushes a tree to a remote cache

    std::filesystem::path workspace = args.size() > 1 ? args.get_value_path(1) : current_path();
    if (workspace.empty()) throw std::invalid_argument("missing workspace argument");

    fstree::ignore_list ignores;
    try {
      ignores.load((workspace / ignorefile).string());
    }
    catch (const std::exception& e) {
    }

    std::unique_ptr<fstree::remote> remote = fstree::remote::create(remoteurl);
    fstree::index index(workspace, ignores);

    try {
      index.load(indexfile);
    }
    catch (...) {
    }

    index.refresh();
    cache.add(index);
    cache.push(index, *remote);
    cache.evict();
    index.save(indexfile);

    std::cout << index.root().hash() << std::endl;
    return EXIT_SUCCESS;
  }
  else {
    throw std::invalid_argument("unknown command: " + args[0]);
  }
}

int main(int argc, char* argv[]) {
  try {
    if (argc < 2) {
      return usage();
    }

    fstree::argparser args;
    args.set_env_prefix("FSTREE");
    args.add_option("--cache", fstree::cache_path().string());
    args.add_option_alias("--cache", "-c");
    args.add_option("--cache-size", "10GiB");
    args.add_option_alias("--cache-size", "-cs");
    args.add_option("--cache-retention", "3600");
    args.add_option_alias("--cache-retention", "-cr");
    args.add_bool_option("--json");
    args.add_option_alias("--json", "-J");
    args.add_option("--ignore", ".fstreeignore");
    args.add_option_alias("--ignore", "-i");
    args.add_option("--index", ".fstree/index");
    args.add_option_alias("--index", "-x");
    args.add_option("--remote", "jolt://localhost:9090");
    args.add_option_alias("--remote", "-r");
    args.add_option("--threads", std::to_string(std::thread::hardware_concurrency()));
    args.add_option_alias("--threads", "-j");
    args.add_bool_option("--help");
    args.add_option_alias("--help", "-h");
    args.add_bool_option("--version");
    args.add_option_alias("--version", "-V");
    args.parse(argc, argv);

    if (args.has_option("--help")) return usage();
    if (args.has_option("--version")) return version();
    if (args.has_option("--json")) fstree::set_events_enabled();

    return cmd_fstree(args);
  }
  catch (const std::exception& e) {
    std::cerr << "error: " << tolower(e.what()) << std::endl;
    return EXIT_FAILURE;
  }
}
