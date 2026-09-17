#pragma once

#include <string>

namespace fstree {

void set_events_enabled();
bool events_enabled();
std::string escape(const std::string& str);
void event(const std::string& type, const std::string& path, const std::string& message = "");
void event(const std::string& type, const std::string& path, size_t value);

}  // namespace fstree
