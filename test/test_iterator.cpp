#include "directory_iterator.hpp"
#include "glob_list.hpp"

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <set>

using namespace fstree;
namespace fs = std::filesystem;

class DirectoryIteratorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary directory for testing
        test_dir = fs::temp_directory_path() / "fstree_test_iterator";
        fs::remove_all(test_dir);
        fs::create_directories(test_dir);
    }

    void TearDown() override {
        // Clean up test directory
        fs::remove_all(test_dir);
    }

    void CreateFile(const fs::path& relative_path, const std::string& content = "test content") {
        fs::path full_path = test_dir / relative_path;
        fs::create_directories(full_path.parent_path());
        std::ofstream file(full_path);
        file << content;
    }

    void CreateDirectory(const fs::path& relative_path) {
        fs::create_directories(test_dir / relative_path);
    }

    void CreateSymlink(const fs::path& relative_path, const fs::path& target) {
        fs::path full_path = test_dir / relative_path;
        fs::create_directories(full_path.parent_path());
        std::error_code ec;
        fs::create_symlink(target, full_path, ec);
        // Skip test if symlinks not supported
        if (ec) {
            GTEST_SKIP() << "Symlinks not supported on this system";
        }
    }

    std::set<std::string> GetPaths(const sorted_directory_iterator& it) {
        std::set<std::string> paths;
        for (const auto& inode : const_cast<sorted_directory_iterator&>(it)) {
            std::string path = inode->path();
            paths.insert(NormalizePath(path));
        }
        return paths;
    }

    std::string NormalizePath(const std::string& path) {
        std::string result = path;
        for (auto& c : result) {
            if (c == '\\') c = '/';
        }
        return result;
    }

    fs::path test_dir;
};

// Test basic functionality
TEST_F(DirectoryIteratorTest, EmptyDirectory) {
    glob_list ignores;
    sorted_directory_iterator it(test_dir, ignores);
    
    EXPECT_EQ(it.begin(), it.end());
    EXPECT_NE(it.root(), nullptr);
    EXPECT_TRUE(it.root()->is_directory());
}

TEST_F(DirectoryIteratorTest, SingleFile) {
    CreateFile("test.txt");
    
    glob_list ignores;
    sorted_directory_iterator it(test_dir, ignores);
    
    auto paths = GetPaths(it);
    EXPECT_EQ(paths.size(), 1);
    EXPECT_EQ(paths.count("test.txt"), 1);
    
    auto first_inode = *it.begin();
    EXPECT_TRUE(first_inode->is_file());
    EXPECT_EQ(first_inode->name(), "test.txt");
}

TEST_F(DirectoryIteratorTest, MultipleFiles) {
    CreateFile("file1.txt");
    CreateFile("file2.txt");
    CreateFile("file3.txt");
    
    glob_list ignores;
    sorted_directory_iterator it(test_dir, ignores);
    
    auto paths = GetPaths(it);
    EXPECT_EQ(paths.size(), 3);
    EXPECT_EQ(paths.count("file1.txt"), 1);
    EXPECT_EQ(paths.count("file2.txt"), 1);
    EXPECT_EQ(paths.count("file3.txt"), 1);
}

TEST_F(DirectoryIteratorTest, SortedByPath) {
    CreateFile("c.txt");
    CreateFile("a.txt");
    CreateFile("b.txt");
    
    glob_list ignores;
    sorted_directory_iterator it(test_dir, ignores);
    
    std::vector<std::string> paths;
    for (const auto& inode : it) {
        std::string path = inode->path();
        // Normalize backslashes to forward slashes
        for (auto& c : path) {
            if (c == '\\') c = '/';
        }
        paths.push_back(path);
    }
    
    EXPECT_EQ(paths, std::vector<std::string>({"a.txt", "b.txt", "c.txt"}));
}

// Test directory recursion
TEST_F(DirectoryIteratorTest, RecursiveDirectories) {
    CreateDirectory("subdir");
    CreateFile("subdir/file1.txt");
    CreateFile("file2.txt");
    CreateDirectory("subdir/nested");
    CreateFile("subdir/nested/file3.txt");
    
    glob_list ignores;
    sorted_directory_iterator it(test_dir, ignores);
    
    auto paths = GetPaths(it);
    EXPECT_EQ(5u, paths.size());
    EXPECT_EQ(1, paths.count("file2.txt"));
    EXPECT_EQ(1, paths.count("subdir/file1.txt"));
    EXPECT_EQ(1, paths.count("subdir/nested/file3.txt"));
}

TEST_F(DirectoryIteratorTest, NonRecursive) {
    CreateDirectory("subdir");
    CreateFile("subdir/file1.txt");
    CreateFile("file2.txt");
    
    glob_list ignores;
    sorted_directory_iterator it(test_dir, ignores, false); // non-recursive
    
    auto paths = GetPaths(it);
    EXPECT_EQ(2u, paths.size());
    EXPECT_EQ(1, paths.count("subdir"));
    EXPECT_EQ(0, paths.count("subdir/file1.txt"));
    EXPECT_EQ(1, paths.count("file2.txt"));
}

// Test ignore patterns
TEST_F(DirectoryIteratorTest, IgnorePatterns) {
    CreateFile("file1.txt");
    CreateFile("file2.log");
    CreateFile("temp.tmp");
    CreateDirectory("logs");
    CreateFile("logs/app.log");
    
    glob_list ignores;
    ignores.add("*.log");
    ignores.add("*.tmp");
    ignores.finalize();
    
    sorted_directory_iterator it(test_dir, ignores);
    
    auto paths = GetPaths(it);
    EXPECT_EQ(2u, paths.size());
    EXPECT_EQ(1, paths.count("file1.txt"));
    EXPECT_EQ(0, paths.count("file2.log"));
    EXPECT_EQ(0, paths.count("temp.tmp"));
    EXPECT_EQ(1, paths.count("logs"));
    EXPECT_EQ(0, paths.count("logs/app.log"));
}

TEST_F(DirectoryIteratorTest, IgnoreDirectories) {
    CreateDirectory("build");
    CreateFile("build/output.bin");
    CreateDirectory("src");
    CreateFile("src/main.cpp");
    CreateFile("README.md");
    
    glob_list ignores;
    ignores.add("build");
    ignores.finalize();
    
    sorted_directory_iterator it(test_dir, ignores);
    
    auto paths = GetPaths(it);
    EXPECT_EQ(3u, paths.size());
    EXPECT_EQ(0, paths.count("build"));
    EXPECT_EQ(0, paths.count("build/output.bin"));
    EXPECT_EQ(1, paths.count("src"));
    EXPECT_EQ(1, paths.count("src/main.cpp"));
    EXPECT_EQ(1, paths.count("README.md"));
}

TEST_F(DirectoryIteratorTest, IgnoreNestedPaths) {
    CreateDirectory("project/node_modules");
    CreateFile("project/node_modules/package.json");
    CreateFile("project/src/main.js");
    CreateFile("project/package.json");
    
    glob_list ignores;
    ignores.add("node_modules");
    ignores.finalize();
    
    sorted_directory_iterator it(test_dir, ignores);
    
    auto paths = GetPaths(it);
    EXPECT_EQ(4u, paths.size());
    EXPECT_EQ(1, paths.count("project"));
    EXPECT_EQ(1, paths.count("project/src"));
    EXPECT_EQ(1, paths.count("project/src/main.js"));
    EXPECT_EQ(1, paths.count("project/package.json"));
    EXPECT_EQ(0, paths.count("project/node_modules"));
    EXPECT_EQ(0, paths.count("project/node_modules/package.json"));
}

// Test symlinks (if supported)
TEST_F(DirectoryIteratorTest, SymlinkFiles) {
    CreateFile("target.txt", "target content");
    CreateSymlink("link.txt", "target.txt");
    
    glob_list ignores;
    sorted_directory_iterator it(test_dir, ignores);
    
    auto paths = GetPaths(it);
    EXPECT_EQ(paths.size(), 2);
    EXPECT_EQ(paths.count("target.txt"), 1);
    EXPECT_EQ(paths.count("link.txt"), 1);
    
    // Find the symlink inode
    inode::ptr link_inode = nullptr;
    for (const auto& inode : it) {
        if (NormalizePath(inode->path()) == "link.txt") {
            link_inode = inode;
            break;
        }
    }
    
    ASSERT_NE(link_inode, nullptr);
    EXPECT_TRUE(link_inode->is_symlink());
    EXPECT_EQ(NormalizePath(link_inode->target()), "target.txt");
}

TEST_F(DirectoryIteratorTest, SymlinkDirectories) {
    CreateDirectory("target_dir");
    CreateFile("target_dir/file.txt");
    CreateSymlink("link_dir", "target_dir");
    
    glob_list ignores;
    sorted_directory_iterator it(test_dir, ignores);
    
    auto paths = GetPaths(it);
    EXPECT_EQ(3u, paths.size());
    EXPECT_EQ(1u, paths.count("target_dir"));
    EXPECT_EQ(1u, paths.count("target_dir/file.txt"));
    EXPECT_EQ(1u, paths.count("link_dir"));
    
    // Should not follow symlinked directories for content
    EXPECT_EQ(0u, paths.count("link_dir/file.txt"));
}

// Test custom compare functions
TEST_F(DirectoryIteratorTest, CustomCompareFunction) {
    CreateFile("1_first.txt");
    CreateFile("2_second.txt");
    CreateFile("3_third.txt");
    
    glob_list ignores;
    
    // Sort by reverse path order
    auto reverse_compare = [](const inode::ptr& a, const inode::ptr& b) {
        return a->path() > b->path();
    };
    
    sorted_directory_iterator it(test_dir, ignores, reverse_compare, false);
    
    std::vector<std::string> paths;
    for (const auto& inode : it) {
        std::string path = inode->path();
        // Normalize backslashes to forward slashes
        for (auto& c : path) {
            if (c == '\\') c = '/';
        }
        paths.push_back(path);
    }
    
    EXPECT_EQ(paths, std::vector<std::string>({"3_third.txt", "2_second.txt", "1_first.txt"}));
}

TEST_F(DirectoryIteratorTest, CompareBySize) {
    CreateFile("small.txt", "a");
    CreateFile("medium.txt", "abc");
    CreateFile("large.txt", "abcdefghij");
    
    glob_list ignores;
    
    // Sort by file size (ascending)
    auto size_compare = [](const inode::ptr& a, const inode::ptr& b) {
        return a->size() < b->size();
    };
    
    sorted_directory_iterator it(test_dir, ignores, size_compare, false);
    
    std::vector<std::string> paths;
    for (const auto& inode : it) {
        std::string path = inode->path();
        // Normalize backslashes to forward slashes
        for (auto& c : path) {
            if (c == '\\') c = '/';
        }
        paths.push_back(path);
    }
    
    EXPECT_EQ(paths, std::vector<std::string>({"small.txt", "medium.txt", "large.txt"}));
}

// Test iterator interface
TEST_F(DirectoryIteratorTest, IteratorInterface) {
    CreateFile("file1.txt");
    CreateFile("file2.txt");
    CreateFile("file3.txt");
    
    glob_list ignores;
    sorted_directory_iterator it(test_dir, ignores);
    
    // Test begin/end
    EXPECT_NE(it.begin(), it.end());
    
    // Test range-based for loop
    int count = 0;
    for (const auto& inode : it) {
        EXPECT_NE(inode, nullptr);
        EXPECT_TRUE(inode->is_file());
        count++;
    }
    EXPECT_EQ(count, 3);
    
    // Test manual iteration
    auto it_begin = it.begin();
    auto it_end = it.end();
    count = 0;
    for (auto iter = it_begin; iter != it_end; ++iter) {
        EXPECT_NE(*iter, nullptr);
        count++;
    }
    EXPECT_EQ(count, 3);
}

TEST_F(DirectoryIteratorTest, RootAccess) {
    CreateFile("test.txt");
    
    glob_list ignores;
    sorted_directory_iterator it(test_dir, ignores);
    
    auto root = it.root();
    EXPECT_NE(root, nullptr);
    EXPECT_TRUE(root->is_directory());
    
    // Test const version
    const auto& const_it = it;
    const auto const_root = const_it.root();
    EXPECT_NE(const_root, nullptr);
    EXPECT_TRUE(const_root->is_directory());
    EXPECT_EQ(root, const_root);
}

// Test edge cases
TEST_F(DirectoryIteratorTest, SpecialFiles) {
    // Create files that should be ignored by default
    CreateFile(".fstree", "fstree config");
    CreateFile("normal.txt");
    
    glob_list ignores;
    sorted_directory_iterator it(test_dir, ignores);
    
    auto paths = GetPaths(it);
    EXPECT_EQ(paths.size(), 1);
    EXPECT_EQ(paths.count("normal.txt"), 1);
    EXPECT_EQ(paths.count(".fstree"), 0);
}

TEST_F(DirectoryIteratorTest, DeepNesting) {
    // Test deeply nested directory structure
    CreateDirectory("a/b/c/d/e");
    CreateFile("a/b/c/d/e/deep.txt");
    CreateFile("a/shallow.txt");
    
    glob_list ignores;
    sorted_directory_iterator it(test_dir, ignores);
    
    auto paths = GetPaths(it);
    EXPECT_EQ(7u, paths.size());
    EXPECT_EQ(1, paths.count("a"));
    EXPECT_EQ(1, paths.count("a/shallow.txt"));
    EXPECT_EQ(1, paths.count("a/b"));
    EXPECT_EQ(1, paths.count("a/b/c"));
    EXPECT_EQ(1, paths.count("a/b/c/d"));
    EXPECT_EQ(1, paths.count("a/b/c/d/e"));
    EXPECT_EQ(1, paths.count("a/b/c/d/e/deep.txt"));
}

TEST_F(DirectoryIteratorTest, MixedFileTypes) {
    CreateFile("regular.txt");
    CreateDirectory("subdir");
    CreateFile("subdir/nested.txt");
    CreateFile("executable.sh");
    // Set executable permissions if possible
    std::error_code ec;
    fs::permissions(test_dir / "executable.sh", 
                   fs::perms::owner_all | fs::perms::group_read | fs::perms::others_read, ec);
    
    glob_list ignores;
    sorted_directory_iterator it(test_dir, ignores);
    
    auto paths = GetPaths(it);
    EXPECT_GE(paths.size(), 3);
    EXPECT_EQ(paths.count("regular.txt"), 1);
    EXPECT_EQ(paths.count("subdir/nested.txt"), 1);
    EXPECT_EQ(paths.count("executable.sh"), 1);
    
    // Verify file types
    for (const auto& inode : it) {
        std::string normalized_path = NormalizePath(inode->path());
        if (normalized_path == "regular.txt" || normalized_path == "executable.sh") {
            EXPECT_TRUE(inode->is_file());
        } else if (normalized_path == "subdir/nested.txt") {
            EXPECT_TRUE(inode->is_file());
        }
    }
}

// Test error conditions
TEST_F(DirectoryIteratorTest, NonExistentDirectory) {
    glob_list ignores;
    
    EXPECT_THROW(
        sorted_directory_iterator(test_dir / "nonexistent", ignores),
        std::runtime_error
    );
}

TEST_F(DirectoryIteratorTest, FileInsteadOfDirectory) {
    CreateFile("notadir.txt");
    
    glob_list ignores;
    
    EXPECT_THROW(
        sorted_directory_iterator(test_dir / "notadir.txt", ignores),
        std::runtime_error
    );
}

// Performance and stress tests
TEST_F(DirectoryIteratorTest, ManyFiles) {
    // Create many files to test performance
    const int num_files = 1000;
    for (int i = 0; i < num_files; ++i) {
        CreateFile("file" + std::to_string(i) + ".txt");
    }
    
    glob_list ignores;
    sorted_directory_iterator it(test_dir, ignores);
    
    auto paths = GetPaths(it);
    EXPECT_EQ(paths.size(), num_files);
    
    // Verify all files are present and sorted
    std::vector<std::string> path_vector(paths.begin(), paths.end());
    for (int i = 0; i < num_files; ++i) {
        std::string expected = "file" + std::to_string(i) + ".txt";
        EXPECT_EQ(paths.count(expected), 1);
    }
}

TEST_F(DirectoryIteratorTest, ComplexIgnorePatterns) {
    CreateFile("src/main.cpp");
    CreateFile("src/test.cpp");
    CreateFile("build/main.o");
    CreateFile("build/test.o");
    CreateFile("docs/readme.txt");
    CreateFile("temp/cache.tmp");
    CreateFile(".git/config");
    CreateFile(".gitignore");
    
    glob_list ignores;
    ignores.add("build");
    ignores.add("*.tmp");
    ignores.add("temp");
    ignores.add(".git");
    ignores.finalize();
    
    sorted_directory_iterator it(test_dir, ignores);
    
    auto paths = GetPaths(it);
    std::set<std::string> expected = {
        "src",
        "src/main.cpp",
        "src/test.cpp", 
        "docs",
        "docs/readme.txt",
        ".gitignore"
    };
    
    EXPECT_EQ(expected, paths);
}

TEST_F(DirectoryIteratorTest, InodeParentship) {
    CreateDirectory("parent/child/grandchild");
    CreateFile("parent/child/grandchild/file.txt");
    
    glob_list ignores;
    sorted_directory_iterator it(test_dir, ignores);
    
    inode::ptr parent_inode = nullptr;
    inode::ptr child_inode = nullptr;
    inode::ptr grandchild_inode = nullptr;
    inode::ptr file_inode = nullptr;
    
    for (const auto& inode : it) {
        std::string normalized_path = NormalizePath(inode->path());
        if (normalized_path == "parent") {
            parent_inode = inode;
        } else if (normalized_path == "parent/child") {
            child_inode = inode;
        } else if (normalized_path == "parent/child/grandchild") {
            grandchild_inode = inode;
        } else if (normalized_path == "parent/child/grandchild/file.txt") {
            file_inode = inode;
        }
    }
    
    ASSERT_NE(parent_inode, nullptr);
    ASSERT_NE(child_inode, nullptr);
    ASSERT_NE(grandchild_inode, nullptr);
    ASSERT_NE(file_inode, nullptr);
    
    EXPECT_EQ(child_inode->parent(), parent_inode);
    EXPECT_EQ(grandchild_inode->parent(), child_inode);
    EXPECT_EQ(file_inode->parent(), grandchild_inode);
}

TEST_F(DirectoryIteratorTest, InodeChildren) {
    CreateDirectory("dir/subdir");
    CreateFile("dir/file1.txt");
    CreateFile("dir/subdir/file2.txt");
    
    glob_list ignores;
    sorted_directory_iterator it(test_dir, ignores);
    
    inode::ptr dir_inode = nullptr;
    for (const auto& inode : it) {
        if (NormalizePath(inode->path()) == "dir") {
            dir_inode = inode;
            break;
        }
    }
    
    ASSERT_NE(dir_inode, nullptr);
    
    std::set<std::string> child_paths;
    for (const auto& child : *dir_inode) {
        child_paths.insert(NormalizePath(child->path()));
    }
    
    EXPECT_EQ(child_paths.size(), 2);
    EXPECT_EQ(child_paths.count("dir/file1.txt"), 1);
    EXPECT_EQ(child_paths.count("dir/subdir"), 1);
}
