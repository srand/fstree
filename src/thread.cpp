#include "thread.hpp"

#include <stdexcept>
#include <thread>

static unsigned int max_threads = std::thread::hardware_concurrency();

// Get the maximum number of threads to use
unsigned int fstree::hardware_concurrency() { return max_threads; }

// Set the maximum number of threads to use
void fstree::set_hardware_concurrency(unsigned threads) {
  max_threads = threads;
  if (max_threads < 1) throw std::invalid_argument("invalid thread count: " + threads);
  if (max_threads > std::thread::hardware_concurrency())
    throw std::invalid_argument("invalid thread count: " + threads);
}
