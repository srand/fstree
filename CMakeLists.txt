cmake_minimum_required(VERSION 3.21)
project(fstree)

################################################################################
# Configuration options
################################################################################

option(fstree_BUILD_TESTS "Build tests" ON)
option(fstree_ENABLE_HTTP "Enable HTTP remote support" ON)
set(fstree_HASH_ALGORITHM "blake3" CACHE STRING "Hash algorithm to use (blake3, sha1)")

################################################################################

if (NOT fstree_HASH_ALGORITHM STREQUAL "sha1" AND NOT fstree_HASH_ALGORITHM STREQUAL "blake3")
    message(FATAL_ERROR "Invalid fstree_HASH_ALGORITHM: ${fstree_HASH_ALGORITHM}. Supported values are 'sha1' and 'blake3'.")
endif()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  set(CMAKE_CXX_FLAGS "/EHsc")
  set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set(CMAKE_CXX_FLAGS_DEBUG "-g")
  set(CMAKE_CXX_FLAGS_RELEASE "-O3 -Wall -Wextra -Werror -pedantic -DNDEBUG")
  set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
endif()

# Add hash algorithm definition
if (fstree_HASH_ALGORITHM STREQUAL "blake3")
    add_compile_definitions(FSTREE_HASH_BLAKE3)
elseif (fstree_HASH_ALGORITHM STREQUAL "sha1")
    add_compile_definitions(FSTREE_HASH_SHA1)
endif()

set(CMAKE_CXX_STANDARD 20)


################################################################################
# Find dependencies
################################################################################

find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
if (fstree_ENABLE_HTTP)
    find_package(CURL CONFIG REQUIRED)
    message(STATUS "HTTP remote support enabled")
endif()
if (${fstree_HASH_ALGORITHM} STREQUAL "blake3")
    find_package(blake3 CONFIG REQUIRED)
    message(STATUS "Using BLAKE3 hash algorithm")
endif()


################################################################################
# Build library
################################################################################

set(SRCS
    src/argparser.cpp
    src/cache.cpp
    src/commit_ostream.cpp
    src/directory_iterator.cpp
    src/event.cpp
    src/glob_list.cpp
    src/hash_${fstree_HASH_ALGORITHM}.cpp
    src/index.cpp
    src/inode.cpp
    src/intrusive_ptr.cpp
    src/jolt.proto
    src/remote.cpp
    src/remote_jolt.cpp
    src/status.cpp
    src/thread.cpp
    src/thread_pool.cpp
)

if (fstree_ENABLE_HTTP)
    add_compile_definitions(FSTREE_ENABLE_HTTP_REMOTE)
    list(APPEND SRCS
        src/remote_http.cpp
    )
endif()

if (WIN32)
    list(APPEND SRCS
        src/directory_iterator_win32.cpp
        src/filesystem_win32.cpp
        src/lock_file_win32.cpp
    )
else()
    list(APPEND SRCS
        src/directory_iterator_posix.cpp
        src/filesystem_posix.cpp
        src/lock_file_posix.cpp
    )
endif()

add_library(fstreelib ${SRCS})

protobuf_generate(
    TARGET
        fstreelib
    LANGUAGE
        cpp
)

# compile the GRPC services
protobuf_generate(
    TARGET
        fstreelib
    LANGUAGE
        grpc
    GENERATE_EXTENSIONS
        .grpc.pb.h
        .grpc.pb.cc
    PLUGIN
        "protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>"
)

target_include_directories(
    fstreelib
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}/src"
)

target_link_libraries(
    fstreelib
    gRPC::grpc++
    protobuf::libprotobuf
)

if (fstree_ENABLE_HTTP)
    target_link_libraries(
        fstreelib
        CURL::libcurl
    )
endif()

if (${fstree_HASH_ALGORITHM} STREQUAL "blake3")
    target_link_libraries(
        fstreelib
        BLAKE3::blake3
    )
endif()


################################################################################
# Build executable
################################################################################

add_executable(fstree src/main.cpp)

target_link_libraries(
    fstree
    PRIVATE
        fstreelib
        protobuf::libprotobuf
)

if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
  target_link_options(
    fstree
    PRIVATE
      "-static"
  )
endif()


install(
    TARGETS
        fstree
    DESTINATION
        bin
)

################################################################################
# Build tests
################################################################################

if (fstree_BUILD_TESTS)
    add_executable(
        fstree_test
        test/test_config.cpp
        test/test_glob.cpp
        test/test_iterator.cpp
        test/test_status.cpp
        test/test_url.cpp
    )

    target_link_libraries(
        fstree_test
        PRIVATE
            fstreelib
            GTest::gtest
            GTest::gtest_main
    )

    add_test(
        NAME
            fstree_test
        COMMAND
            fstree_test
    )

    install(
        TARGETS
            fstree_test
        DESTINATION
            bin
    )
endif()

################################################################################