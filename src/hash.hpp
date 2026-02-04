#pragma once

#include "digest.hpp"

#include <filesystem>
#include <string>

namespace fstree {

// Calculate the hash sum of a stream. The stream is read until EOF.
fstree::digest hashsum_hex(std::istream& stream);

// Calculate the hash sum of a file. The file is read until EOF.
fstree::digest hashsum_hex_file(const std::filesystem::path& path);

#if defined(FSTREE_HASH_BLAKE3)
const fstree::digest::algorithm hash_function = fstree::digest::algorithm::blake3;
const std::string hash_name = "blake3";
constexpr size_t hash_digest_length = 64;
#elif defined(FSTREE_HASH_SHA1)
const fstree::digest::algorithm hash_function = fstree::digest::algorithm::sha1;
const std::string hash_name = "sha1";
constexpr size_t hash_digest_length = 40;
#else
#error "No hash algorithm defined. Define FSTREE_HASH_BLAKE3 or FSTREE_HASH_SHA1."
#endif

}  // namespace fstree
