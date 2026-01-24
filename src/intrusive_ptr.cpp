
#ifndef NDEBUG

#include "intrusive_ptr.hpp"

#include <atomic>
#include <cstdlib>
#include <iostream>

namespace fstree {

static std::atomic<size_t> objects_in_use{0};

void increment_objects_in_use() {
    objects_in_use.fetch_add(1, std::memory_order_relaxed); 
}

void decrement_objects_in_use() {
    objects_in_use.fetch_sub(1, std::memory_order_relaxed); 
}

void report_leaks() {
    size_t count = objects_in_use.load(std::memory_order_relaxed);
    std::cerr << "Leak check: " << count << " objects still in use." << std::endl;
}

// Register atexit handler
struct ObjectLeakChecker {
    ~ObjectLeakChecker() {
        report_leaks();
    }
} g_leak_checker;

}

#endif  // NDEBUG
