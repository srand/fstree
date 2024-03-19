cmake_minimum_required(VERSION 3.21)
project(fstree)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  set(CMAKE_CXX_FLAGS_RELEASE "/O2 /MT")
else()
  set(CMAKE_CXX_FLAGS_DEBUG "-g")
  set(CMAKE_CXX_FLAGS_RELEASE "-O3 -Wall -Wextra -Werror -pedantic")
endif()

set(CMAKE_CXX_STANDARD 20)

find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(absl CONFIG REQUIRED)

set(SRCS
    src/cache.cpp
    src/cache.proto
    src/index.cpp
    src/inode.cpp
    src/main.cpp
    src/remote_grpc.cpp
    src/sha1.cpp
    src/thread.cpp
    src/thread_pool.cpp
)

if (WIN32)
    list(APPEND SRCS
    # src/directory_iterator_std.cpp
    src/directory_iterator_win32.cpp
    src/filesystem_win32.cpp
    )
else()
    list(APPEND SRCS
        src/directory_iterator_posix.cpp
        src/filesystem_posix.cpp
    )
endif()

add_executable(fstree ${SRCS})

target_include_directories(
    fstree
    PUBLIC
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}/src"
)

target_link_libraries(
    fstree
    PRIVATE
        #fstree-proto
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

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  target_link_options(
    fstree
    PRIVATE
      "/MT"
  )
else()
  target_link_options(
    fstree
    PRIVATE
      "-static"
  )
endif()


protobuf_generate(
    TARGET
        fstree
    LANGUAGE
        cpp
)

# compile the GRPC services
protobuf_generate(
    TARGET
        fstree
    LANGUAGE
        grpc
    GENERATE_EXTENSIONS
        .grpc.pb.h
        .grpc.pb.cc
    PLUGIN
        "protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>"
)

install(
    TARGETS
        fstree
    DESTINATION
        bin
)
