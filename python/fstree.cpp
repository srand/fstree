#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include <pybind11/chrono.h>

#include "cache.hpp"
#include "index.hpp"
#include "remote.hpp"
#include "url.hpp"
#include "inode.hpp"
#include "glob_list.hpp"
#include "filesystem.hpp"
#include "simple.hpp"

namespace py = pybind11;

// Tell pybind11 about our intrusive_ptr
PYBIND11_DECLARE_HOLDER_TYPE(T, fstree::intrusive_ptr<T>);

PYBIND11_MODULE(fstree, m) {
    m.doc() = "Python bindings for fstree - filesystem tree sharing and sync";
    
    // Expose the simple API
    py::class_<fstree::simple>(m, "Simple")
        .def(py::init<>())
        .def("write_tree", &fstree::simple::write_tree, py::arg("path"))
        .def("write_tree_push", &fstree::simple::write_tree_push, py::arg("path"), py::arg("remote_url"))
        .def("push", &fstree::simple::push, py::arg("tree_hash"), py::arg("remote_url"))
        .def("pull", &fstree::simple::pull,  py::arg("tree_hash"), py::arg("remote_url"))
        .def("pull_checkout", &fstree::simple::pull_checkout, py::arg("tree_hash"), py::arg("remote_url"), py::arg("dest_path"))
        .def("checkout", &fstree::simple::checkout, py::arg("tree_hash"), py::arg("dest_path"))
        .def("lookup", [](fstree::simple &s, const std::string& path) -> py::object {
            std::string hash;
            if (!s.lookup(path, hash)) {
                // Return None if path not found in index
                return py::none();
            }
            return py::str(hash);
        }, py::arg("path"));
    
    // Expose the glob_list class
    py::class_<fstree::glob_list>(m, "GlobList")
        .def(py::init<>())
        .def(py::init([](const std::string& path) {
            auto il = std::make_unique<fstree::glob_list>();
            il->load(path);
            il->finalize();
            return il;
        }))
        .def(py::init([](const std::vector<std::string>& paths) {
            auto il = std::make_unique<fstree::glob_list>();
            for (const auto& path : paths) {
                il->add(path);
            }
            il->finalize();
            return il;
        }))
        .def("__iter__", [](const fstree::glob_list &il) {
            return py::make_iterator(il.begin(), il.end());
        }, py::keep_alive<0, 1>())
        .def("add", [](fstree::glob_list &il, const std::string& pattern) {
            il.add(pattern);
            il.finalize();
        })
        .def("matches", &fstree::glob_list::match);

    // Expose the inode class using smart pointers
    py::class_<fstree::inode, fstree::intrusive_ptr<fstree::inode>>(m, "Inode")
        .def_property_readonly("name", &fstree::inode::name)
        .def_property_readonly("path", &fstree::inode::path)
        .def_property_readonly("size", &fstree::inode::size)
        .def_property_readonly("mode", [](const fstree::inode& inode) {
            return unsigned(inode.status().permissions());
        })
        .def_property_readonly("hash", [](const fstree::inode& inode) {
            auto hash = inode.hash();
            if (inode.is_symlink()) {
                throw std::runtime_error("inode is a symlink, hash is not applicable");
            }
            if (hash.empty()) {
                throw std::runtime_error("inode has not been cached yet");
            }
            return hash;
        })
        .def_property_readonly("is_directory", &fstree::inode::is_directory)
        .def_property_readonly("is_dirty", &fstree::inode::is_dirty)
        .def_property_readonly("is_file", &fstree::inode::is_file)
        .def_property_readonly("is_ignored", &fstree::inode::is_ignored)
        .def_property_readonly("is_symlink", &fstree::inode::is_symlink)
        .def_property_readonly("mtime", [](const fstree::inode& inode) {
            // Return modification time as datetime object
            return std::chrono::system_clock::time_point(std::chrono::nanoseconds(inode.last_write_time()));
        })
        .def_property_readonly("symlink_target", &fstree::inode::target)
        .def("__iter__", [](fstree::inode &node) {
            return py::make_iterator(node.begin(), node.end());
        }, py::keep_alive<0, 1>());
    
    // Expose the index class
    py::class_<fstree::index>(m, "Index")
        .def(py::init([]() {
            return std::make_unique<fstree::index>();
        }))
        .def(py::init([](const std::string& path) {
            return std::make_unique<fstree::index>(std::filesystem::path(path));
        }))
        .def(py::init([](const std::string& path, const fstree::glob_list& ignore) {
            return std::make_unique<fstree::index>(std::filesystem::path(path), ignore);
        }))
        .def("checkout", [](fstree::index &idx, fstree::cache &cache, const std::string& path) {
            idx.checkout(cache, std::filesystem::path(path));
        })
        .def("load", [](fstree::index &idx) {
            idx.load(std::filesystem::path(".fstree/index"));
        })
        .def("load", [](fstree::index &idx, const std::string& file) {
            idx.load(std::filesystem::path(file));
        })
        .def("refresh", &fstree::index::refresh)
        .def_property_readonly("root", [](fstree::index &idx) -> fstree::intrusive_ptr<fstree::inode> {
            return idx.root();
        })
        .def("save", [](fstree::index &idx) {
            idx.save(std::filesystem::path(".fstree/index"));
        })
        .def("save", [](fstree::index &idx, const std::string& file) {
            idx.save(std::filesystem::path(file));
        })
        .def("__len__", &fstree::index::size)
        .def("__iter__", [](fstree::index &idx) {
            return py::make_iterator(idx.begin(), idx.end());
        }, py::keep_alive<0, 1>());
    
    // Expose the cache class
    py::class_<fstree::cache>(m, "Cache")
        .def(py::init([]() {
            return std::make_unique<fstree::cache>(fstree::cache::default_path(), fstree::cache::default_max_size, fstree::cache::default_retention);
        }))
        .def(py::init([](const std::string& path) {
            return std::make_unique<fstree::cache>(std::filesystem::path(path), fstree::cache::default_max_size, fstree::cache::default_retention);
        }))
        .def(py::init([](const std::string& path, size_t max_size) {
            return std::make_unique<fstree::cache>(std::filesystem::path(path), max_size, fstree::cache::default_retention);
        }))
        .def(py::init([](const std::string& path, size_t max_size, int retention_seconds) {
            return std::make_unique<fstree::cache>(std::filesystem::path(path), max_size, std::chrono::seconds(retention_seconds));
        }))
        .def("has_object", &fstree::cache::has_object)
        .def("has_tree", &fstree::cache::has_tree)
        .def("add", &fstree::cache::add);

    /*
    // Expose the url class
    py::class_<fstree::url>(m, "URL")
        .def(py::init<const std::string&>())
        .def("scheme", &fstree::url::scheme)
        .def("host", &fstree::url::host)  
        .def("path", &fstree::url::path);
    
    // Expose the remote class (as abstract base)
    py::class_<fstree::remote>(m, "Remote")
        .def("has_object", &fstree::remote::has_object)
        .def("has_objects", &fstree::remote::has_objects)
        .def_static("create", &fstree::remote::create);
    */
}
