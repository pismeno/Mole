#include "editor/process_orchestrator.hpp"

#include <functional>
#include <iostream>
#include <iterator>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "editor/subprocess.hpp"
#include "utils/result.hpp"

namespace mole {

ProcessOrchestrator::~ProcessOrchestrator() = default;

Result<void> ProcessOrchestrator::spawn(const std::vector<std::string>& command,
                                std::function<void()> on_finish) {
  auto r_process = Subprocess::create(command);
  if (!r_process) {
    return mole::fail("Failed to start subprocess: " + r_process.error());
  }

  {
    std::lock_guard<std::mutex> lock(pending_mutex);
    pending_jobs.push_back(ProcessJob{
        std::move(r_process).value(),
        std::move(on_finish)
    });
  }

  return mole::ok();
}

void ProcessOrchestrator::update() {
  // Move newly spawned jobs from the staging queue into active_jobs
  {
    std::lock_guard<std::mutex> lock(pending_mutex);
    if (!pending_jobs.empty()) {
      active_jobs.insert(
          active_jobs.end(),
          std::make_move_iterator(pending_jobs.begin()),
          std::make_move_iterator(pending_jobs.end())
      );
      pending_jobs.clear();
    }
  }

  // Defer execution of finished callbacks until after active_jobs iteration
  // so callbacks that call spawn() or modify jobs cannot invalidate iterators.
  std::vector<std::function<void()>> callbacks_to_run;

  for (auto it = active_jobs.begin(); it != active_jobs.end(); ) {
    ProcessJob& job = *it;

    std::vector<std::string> lines = job.process->poll_output();
    for (const std::string& line : lines) {
      std::cout << line << "\n";
    }

    if (!job.process->is_running()) {
      // Drain any final output lines produced upon process completion
      std::vector<std::string> final_lines = job.process->poll_output();
      for (const std::string& line : final_lines) {
        std::cout << line << "\n";
      }

      if (job.on_finish) {
        callbacks_to_run.push_back(std::move(job.on_finish));
      }
      it = active_jobs.erase(it);
    } else {
      ++it;
    }
  }

  // Safely execute completion callbacks outside of the active_jobs loop
  for (const auto& callback : callbacks_to_run) {
    if (callback) {
      callback();
    }
  }

  // Move any jobs spawned from completion callbacks into active_jobs
  {
    std::lock_guard<std::mutex> lock(pending_mutex);
    if (!pending_jobs.empty()) {
      active_jobs.insert(
          active_jobs.end(),
          std::make_move_iterator(pending_jobs.begin()),
          std::make_move_iterator(pending_jobs.end())
      );
      pending_jobs.clear();
    }
  }
}

void ProcessOrchestrator::flush_and_wait() {
  {
    std::lock_guard<std::mutex> lock(pending_mutex);
    if (!pending_jobs.empty()) {
      active_jobs.insert(
          active_jobs.end(),
          std::make_move_iterator(pending_jobs.begin()),
          std::make_move_iterator(pending_jobs.end())
      );
      pending_jobs.clear();
    }
  }

  for (auto& job : active_jobs) {
    job.process->wait();
  }

  for (auto& job : active_jobs) {
    if (job.on_finish) {
      job.on_finish();
    }
  }

  active_jobs.clear();
}

} // mole
