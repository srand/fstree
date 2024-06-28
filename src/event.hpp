#pragma once

#include "commit_ostream.hpp"

#include <ostream>

namespace fstree {

void set_events_enabled();
bool events_enabled();
void event(const std::string& type, const std::string& path, const std::string& message = "");
void event(const std::string& type, const std::string& path, size_t value);

}  // namespace fstree
