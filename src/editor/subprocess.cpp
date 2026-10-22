#include "editor/subprocess.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>

#include <reproc++/drain.hpp>

#include "utils/result.hpp"

namespace mole {

Subprocess::~Subprocess() {
  stop();
}

Result<std::unique_ptr<Subprocess>> Subprocess::create(const std::vector<std::string>& command) {
  std::unique_ptr<Subprocess> subprocess(new Subprocess());

  auto r_start = subprocess->start(command);
  if (!r_start) return mole::fail(r_start.error());

  return std::move(subprocess);
}

Result<void> Subprocess::start(const std::vector<std::string>& command) {
  if (running) {
    return mole::fail("Subprocess is already running");
  }

  reproc::options options;
  // Note: We intentionally leave options.redirect as defaults here to bypass
  // the MSVC `stdout` macro error. We will combine the streams manually in the sink.

  std::error_code ec = process.start(command, options);
  if (ec) {
    std::string err_msg = "[ERROR] Failed to start process: " + ec.message();

    std::lock_guard<std::mutex> lock(mutex);
    output_queue.push(err_msg);
    return mole::fail(ec.message());
  }

  running = true;

  worker_thread = std::thread([this]() {
    // We maintain two separate buffers so that if the compiler writes to out and err
    // simultaneously, the chunks don't interleave and garble the text.
    std::string out_buffer;
    std::string err_buffer;

    auto live_sink = [this, &out_buffer, &err_buffer](reproc::stream stream, const uint8_t* buffer, size_t size) -> std::error_code {
      std::string chunk(reinterpret_cast<const char*>(buffer), size);

      std::lock_guard<std::mutex> lock(mutex);

      // Route the chunk to the correct buffer
      std::string& current_buffer = (stream == reproc::stream::out) ? out_buffer : err_buffer;
      current_buffer += chunk;

      // Flush complete lines to the editor's queue
      size_t pos = 0;
      while ((pos = current_buffer.find('\n')) != std::string::npos) {
        std::string line = current_buffer.substr(0, pos);
        output_queue.push("[Subprocess] " + std::move(line));

        current_buffer.erase(0, pos + 1);
      }

      return std::error_code{}; // Tell reproc to keep reading
    };

    // By passing live_sink twice, we tell reproc to process BOTH stdout and stderr
    reproc::drain(process, live_sink, live_sink);

    // Wait for the OS to release the process and get the exit code
    int status = 0;
    std::error_code wait_ec;
    std::tie(status, wait_ec) = process.wait(reproc::infinite);

    std::lock_guard<std::mutex> lock(mutex);

    // Flush any remaining characters that didn't end in newlines
    if (!out_buffer.empty()) {
      output_queue.push(std::move(out_buffer));
    }
    if (!err_buffer.empty()) {
      output_queue.push(std::move(err_buffer));
    }

    output_queue.push("[Subprocess] finished with exit code " + std::to_string(status));
    running = false;
  });

  return mole::ok();
}

std::vector<std::string> Subprocess::poll_output() {
  std::vector<std::string> lines;
  std::lock_guard<std::mutex> lock(mutex);

  while (!output_queue.empty()) {
    lines.push_back(std::move(output_queue.front()));
    output_queue.pop();
  }

  return lines;
}

void Subprocess::stop() {
  if (running) {
    // Only attempt to stop the OS process if it's still running
    reproc::stop_actions actions = {
        { reproc::stop::terminate, reproc::milliseconds(200) },
        { reproc::stop::kill, reproc::milliseconds(100) }
    };
    process.stop(actions);
  }

  if (worker_thread.joinable()) {
    worker_thread.join();
  }
}

void Subprocess::wait() {
  if (worker_thread.joinable()) {
    worker_thread.join(); // Blocks until the OS process exits and the thread finishes
  }
}

bool Subprocess::is_running() const {
  return running;
}

} // mole
