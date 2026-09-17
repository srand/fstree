#ifndef ENCODING_HPP
#define ENCODING_HPP

#include <filesystem>
#include <string>

namespace fstree {

// fstree stores paths as UTF-8 encoded narrow strings, in the index, in the
// tree and in the event stream, so that the same tree hashes identically on
// every platform. POSIX filenames are opaque byte strings and pass through
// unchanged, but Windows filenames are UTF-16 and the implicit
// std::filesystem::path conversions go through the active code page, which
// silently replaces every character it cannot represent. Convert explicitly at
// each boundary instead of relying on those conversions.
std::filesystem::path to_path(const std::string& utf8);
std::string to_utf8(const std::filesystem::path& path);

#ifdef _WIN32
// Conversions to and from the native UTF-16 encoding, for the wide Win32 entry
// points. Only the narrow entry points go through the active code page, so the
// wide ones are the only way to name a file the code page cannot express.
std::wstring to_wide(const std::string& utf8);
std::string from_wide(const std::wstring& utf16);
#endif

}  // namespace fstree

#endif  // ENCODING_HPP
