#include "event.hpp"

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

// Escape string for JSON
std::string escape(const std::string& str) {
  std::string escaped_str;
  for (char c : str) {
    switch (c) {
      case '\"':
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
        escaped_str += c;
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
