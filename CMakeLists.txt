cmake_minimum_required(VERSION 3.21)
project(fstree)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  set(CMAKE_CXX_FLAGS "/EHsc")
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
  set(CMAKE_CXX_FLAGS_DEBUG "-g")
  set(CMAKE_CXX_FLAGS_RELEASE "-s -O3 -Wall -Wextra -Werror -pedantic")
endif()

set(CMAKE_CXX_STANDARD 20)


################################################################################
# Find dependencies
################################################################################

find_package(Protobuf CONFIG REQUIRED)
find_package(absl CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)


################################################################################
# Build library
################################################################################

set(SRCS
    src/cache.cpp
    src/event.cpp
    src/index.cpp
    src/inode.cpp
    src/jolt.proto
    src/remote.cpp
    src/remote_jolt.cpp
    src/sha1.cpp
    src/thread.cpp
    src/thread_pool.cpp
)

if (WIN32)
    list(APPEND SRCS
        src/directory_iterator_win32.cpp
        src/filesystem_win32.cpp
    )
else()
    list(APPEND SRCS
        src/directory_iterator_posix.cpp
        src/filesystem_posix.cpp
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
    PRIVATE
        gRPC::grpc++
        gRPC::grpc
        gRPC::gpr
        gRPC::upb
        gRPC::crypto
        gRPC::ssl
        gRPC::cares
        gRPC::address_sorting
        gRPC::gpr
        gRPC::re2
        protobuf::libprotobuf
        absl::check
        absl::flags
        absl::flags_parse
        absl::status
        absl::statusor
)


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

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
else()
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

add_executable(
    fstree_test
    test/test_ignore.cpp
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

################################################################################
