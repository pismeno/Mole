#pragma once

#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <wtr/watcher.hpp>

#include "utils/result.hpp"

namespace mole {

struct EditorContext;

enum class FileWatchEventType {
  Created,
  Modified,
  Deleted
};

struct FileWatchEvent {
  std::filesystem::path path;
  FileWatchEventType type;
};

class FileWatcher {
 public:
  ~FileWatcher() = default;

  std::vector<FileWatchEvent> poll_events();

  static Result<std::unique_ptr<FileWatcher>> create(std::string_view watch_path, const EditorContext& editor_context);

 private:
  explicit FileWatcher(std::filesystem::path watch_path);

  void start();
  void push_event(std::filesystem::path path, FileWatchEventType type);

  struct PendingEvent {
    FileWatchEventType type;
    std::chrono::steady_clock::time_point last_event_time;
  };

  std::filesystem::path watch_path;

  std::unique_ptr<wtr::watch> watcher;
  std::mutex mutex;
  std::unordered_map<std::filesystem::path, PendingEvent> pending_events;
  static constexpr std::chrono::milliseconds debounce_delay{100};
};

} // mole
