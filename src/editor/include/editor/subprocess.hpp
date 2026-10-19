#pragma once

#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <memory>
#include <thread>
#include <atomic>

#include <reproc++/reproc.hpp>

#include "utils/result.hpp"

namespace mole {

class Subprocess {
 public:
  ~Subprocess();

  std::vector<std::string> poll_output();

  void stop();
  void wait();
  bool is_running() const;

  static Result<std::unique_ptr<Subprocess>> create(const std::vector<std::string>& command);

 private:
  explicit Subprocess() = default;

  Result<void> start(const std::vector<std::string>& command);

  reproc::process process;
  std::thread worker_thread;

  std::mutex mutex;
  std::queue<std::string> output_queue;

  std::atomic<bool> running{false};

  // Buffer used by the background thread to assemble fragmented pipe chunks 
  // into clean, unbroken lines before pushing to the queue.
  std::string line_buffer;
};

} // mole