#pragma once

#include <filesystem>
#include <string>

namespace fstree {

// Calculate the hash sum of a stream. The stream is read until EOF.
std::string hashsum_hex(std::istream& stream);

// Calculate the hash sum of a file. The file is read until EOF.
std::string hashsum_hex_file(const std::filesystem::path& path);

#if defined(FSTREE_HASH_BLAKE3)
const std::string hash_name = "blake3";
constexpr size_t hash_digest_len = 64;
#elif defined(FSTREE_HASH_SHA1)
const std::string hash_name = "sha1";
constexpr size_t hash_digest_len = 40;
#else
#error "No hash algorithm defined. Define FSTREE_HASH_BLAKE3 or FSTREE_HASH_SHA1."
#endif

}  // namespace fstree
