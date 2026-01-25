#include "hash.hpp"

#include <blake3.h>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>


namespace fstree {

// Calculate the hash sum of a stream. The stream is read until EOF.
std::string hashsum_hex(std::istream& stream) {
  // Initialize the hasher.
  blake3_hasher hasher;
  blake3_hasher_init(&hasher);

  // Read the stream in chunks.
  constexpr size_t buffer_size = 64 * 1024;
  char buffer[buffer_size];

  while (stream) {
    stream.read(buffer, buffer_size);
    std::streamsize bytes_read = stream.gcount();
    if (bytes_read > 0) {
      blake3_hasher_update(&hasher, buffer, static_cast<size_t>(bytes_read));
    }
  }

  // Finalize the hash.
  uint8_t hash_output[BLAKE3_OUT_LEN];
  blake3_hasher_finalize(&hasher, hash_output, BLAKE3_OUT_LEN);

  // Convert the hash to a hexadecimal string.
  std::ostringstream oss;
  oss << std::hex << std::setfill('0');
  for (size_t i = 0; i < BLAKE3_OUT_LEN; i++) {
    oss << std::setw(2) << static_cast<int>(hash_output[i]);
  }
  return oss.str();
}

// Calculate the hash sum of a file. The file is read until EOF.
std::string hashsum_hex_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("failed to open file: " + path.string() + ": " + std::strerror(errno));
    }
    return hashsum_hex(file);
}

}  // namespace fstree
