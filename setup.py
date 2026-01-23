#!/usr/bin/env python3

from setuptools import setup, Extension
from pybind11.setup_helpers import Pybind11Extension, build_ext
import pybind11


# Add our source directories
include_dirs = [
    'src',
    'build',  # For generated protobuf files
    'build/src',
    pybind11.get_cmake_dir() + '/../include'
]

# Define the extension
ext_modules = [
    Pybind11Extension(
        "fstree",
        [
            "python/fstree_python.cpp",
            "src/cache.cpp",
            "src/event.cpp", 
            "src/index.cpp",
            "src/inode.cpp",
            "src/sha1.cpp",
            "src/thread.cpp",
            "src/thread_pool.cpp",
            "src/directory_iterator_posix.cpp",
            "src/filesystem_posix.cpp",
            "src/lock_file_posix.cpp",

        ],
        include_dirs=include_dirs,
        libraries=[],
        library_dirs=[],
        language='c++',
        cxx_std=20,
    ),
]

setup(
    name="fstree",
    version="0.1.0",
    author="Your Name",
    author_email="your.email@example.com",
    description="Python bindings for fstree - filesystem tree sharing and sync",
    long_description="",
    ext_modules=ext_modules,
    cmdclass={"build_ext": build_ext},
    zip_safe=False,
    python_requires=">=3.7",
    install_requires=[
        "pybind11",
    ],
)
