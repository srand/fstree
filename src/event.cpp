#include "event.hpp"

#include "commit_ostream.hpp"

#include <iostream>
#include <mutex>
#include <ostream>
#include <string>

namespace fstree {

static bool _events_enabled = false;
static std::ostream& _stream(std::cerr);
static std::mutex _mutex;

void set_events_enabled() { _events_enabled = true; }
bool events_enabled() { return _events_enabled; }

namespace {

constexpr char hex_digit(unsigned int value) { return "0123456789abcdef"[value & 0x0f]; }

void append_byte_escape(std::string& output, unsigned char byte) {
  output += "\\u00";
  output += hex_digit(byte >> 4);
  output += hex_digit(byte);
}

}  // namespace

// Escape arbitrary bytes as a reversible JSON string payload. Printable ASCII
// is preserved and every other byte is represented as a \u00XX escape.
std::string escape(const std::string& str) {
  std::string escaped_str;
  escaped_str.reserve(str.size());
  for (unsigned char c : str) {
    switch (c) {
      case '"':
        escaped_str += "\\\"";
        break;
      case '\\':
        escaped_str += "\\\\";
        break;
      case '\b':
        escaped_str += "\\b";
        break;
      case '\f':
        escaped_str += "\\f";
        break;
      case '\n':
        escaped_str += "\\n";
        break;
      case '\r':
        escaped_str += "\\r";
        break;
      case '\t':
        escaped_str += "\\t";
        break;
      default:
        if (c < 0x20 || c >= 0x7f) {
          append_byte_escape(escaped_str, c);
        } else {
          escaped_str += static_cast<char>(c);
        }
        break;
    }
  }
  return escaped_str;
}

// Emit an event in JSON format
void event(const std::string& type, const std::string& path, const std::string& message) {
  if (!_events_enabled) {
    return;
  }

  commit_ostream stream([](const std::string& message) {
    std::lock_guard<std::mutex> lock(_mutex);
    _stream << message;
  });
  stream << "{ \"type\": \"" << type << "\", \"path\": \"" << escape(path) << "\"";
  if (!message.empty()) {
    stream << ", \"message\": \"" << escape(message) << "\"";
  }
  stream << " }\n";
}

void event(const std::string& type, const std::string& path, size_t value) {
  if (!_events_enabled) {
    return;
  }

  commit_ostream stream([](const std::string& message) {
    std::lock_guard<std::mutex> lock(_mutex);
    _stream << message;
  });
  stream << "{ \"type\": \"" << type << "\", \"path\": \"" << escape(path) << "\", \"value\": " << value << " }\n";
}

}  // namespace fstree
