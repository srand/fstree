#pragma once

#include <filesystem>
#include <string>
#include <vector>

// Calculate the SHA-1 hash of a stream. The stream is read until EOF.
std::string sha1_hex(std::istream& stream);

// Calculate the SHA-1 hash of a file. The file is read until EOF.
std::string sha1_hex_file(const std::filesystem::path& path);
