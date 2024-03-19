
// Implementation of the SHA-1 hash algorithm
// Based on the implementation in the Git source code
//

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// Calculate the SHA-1 hash of a stream
std::string sha1_hex(std::istream& stream) {
  // Read file contents
  std::vector<unsigned char> buffer(std::istreambuf_iterator<char>(stream), {});

  // Initialize SHA1 variables
  uint32_t h0 = 0x67452301;
  uint32_t h1 = 0xEFCDAB89;
  uint32_t h2 = 0x98BADCFE;
  uint32_t h3 = 0x10325476;
  uint32_t h4 = 0xC3D2E1F0;

  // Append padding
  uint64_t totalBits = buffer.size() * 8;

  buffer.push_back(0x80);
  while ((buffer.size() * 8) % 512 != 448) {
    buffer.push_back(0x00);
  }

  buffer.resize(buffer.size() + 8);
  for (int i = 0; i < 8; i++) {
    buffer[buffer.size() - 8 + i] = (totalBits >> (56 - i * 8)) & 0xFF;
  }

  // Process blocks
  for (size_t i = 0; i < buffer.size(); i += 64) {
    std::vector<uint32_t> words(80);
    for (int j = 0; j < 16; j++) {
      words[j] = (buffer[i + j * 4] << 24) | (buffer[i + j * 4 + 1] << 16) | (buffer[i + j * 4 + 2] << 8) |
                 (buffer[i + j * 4 + 3]);
    }

    for (int j = 16; j < 80; j++) {
      words[j] = (words[j - 3] ^ words[j - 8] ^ words[j - 14] ^ words[j - 16]);
      words[j] = (words[j] << 1) | (words[j] >> 31);
    }

    uint32_t a = h0;
    uint32_t b = h1;
    uint32_t c = h2;
    uint32_t d = h3;
    uint32_t e = h4;

    for (int j = 0; j < 80; j++) {
      uint32_t f, k;
      if (j < 20) {
        f = (b & c) | ((~b) & d);
        k = 0x5A827999;
      }
      else if (j < 40) {
        f = b ^ c ^ d;
        k = 0x6ED9EBA1;
      }
      else if (j < 60) {
        f = (b & c) | (b & d) | (c & d);
        k = 0x8F1BBCDC;
      }
      else {
        f = b ^ c ^ d;
        k = 0xCA62C1D6;
      }

      uint32_t temp = ((a << 5) | (a >> 27)) + f + e + k + words[j];
      e = d;
      d = c;
      c = (b << 30) | (b >> 2);
      b = a;
      a = temp;
    }

    h0 += a;
    h1 += b;
    h2 += c;
    h3 += d;
    h4 += e;
  }

  // Convert hash to hexadecimal string
  std::ostringstream oss;
  oss << std::hex << std::setfill('0');
  oss << std::setw(8) << h0 << std::setw(8) << h1 << std::setw(8) << h2 << std::setw(8) << h3 << std::setw(8) << h4;

  return oss.str();
}

// Calculate the SHA-1 hash of a file
std::string sha1_hex_file(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    throw std::runtime_error("failed to open file: " + path.string() + ": " + std::strerror(errno));
  }

  return sha1_hex(file);
}
