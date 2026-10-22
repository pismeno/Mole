#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "utils/result.hpp"

namespace mole {

class Subprocess;

class ProcessOrchestrator {
 public:
  explicit ProcessOrchestrator() = default;
  ~ProcessOrchestrator();

  /// Spawns a new background process and registers it for tracking
  Result<void> spawn(const std::vector<std::string>& command, std::function<void()> on_finish = nullptr);

  /// Blocks the calling thread until all subprocesses finish, then executes their callbacks.
  void flush_and_wait();

  /// Call this once per frame in your main engine loop
  void update();

 private:
  struct ProcessJob {
    std::unique_ptr<Subprocess> process;
    std::function<void()> on_finish;
  };

  std::vector<ProcessJob> active_jobs;
  std::vector<ProcessJob> pending_jobs;
  std::mutex pending_mutex;
};

} // mole
