#include "editor/file_watcher.hpp"

#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string_view>
#include <utility>
#include <vector>

#include <wtr/watcher.hpp>

#include "editor/editor_context.hpp"
#include "osal/ifile_system.hpp"
#include "osal/os_context.hpp"
#include "utils/result.hpp"

namespace mole {

FileWatcher::FileWatcher(std::filesystem::path watch_path)
    : watch_path(std::move(watch_path)) {}

Result<std::unique_ptr<FileWatcher>> FileWatcher::create(std::string_view watch_path, const EditorContext& context) {
  auto r_resolve_path = context.os_context->file_system->resolve(watch_path);
  if (!r_resolve_path) return mole::fail(r_resolve_path.error());

  std::unique_ptr<FileWatcher> watcher(new FileWatcher(std::move(r_resolve_path).value()));
  watcher->start();

  return std::move(watcher);
}

void FileWatcher::start() {
  watcher = std::make_unique<wtr::watch>(watch_path, [this](wtr::event const& event) {
    if (event.path_type == wtr::event::path_type::file) {
      if (event.effect_type == wtr::event::effect_type::modify) {
        push_event(event.path_name, FileWatchEventType::Modified);
      }
      else if (event.effect_type == wtr::event::effect_type::create) {
        push_event(event.path_name, FileWatchEventType::Created);
      }
      else if (event.effect_type == wtr::event::effect_type::destroy) {
        push_event(event.path_name, FileWatchEventType::Deleted);
      }
    }
  });
}

std::vector<FileWatchEvent> FileWatcher::poll_events() {
  std::lock_guard<std::mutex> lock(mutex);
  std::vector<FileWatchEvent> events_to_process;
  auto now = std::chrono::steady_clock::now();

  for (auto it = pending_events.begin(); it != pending_events.end();) {
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second.last_event_time);
    if (elapsed >= debounce_delay) {
      events_to_process.push_back({it->first, it->second.type});
      it = pending_events.erase(it);
    } else {
      ++it;
    }
  }

  return events_to_process;
}

void FileWatcher::push_event(std::filesystem::path path, FileWatchEventType type) {
  std::lock_guard<std::mutex> lock(mutex);

  auto it = pending_events.find(path);
  if (it != pending_events.end()) {
    // Preserve Created status if follow-up modification arrives in the same burst
    if (it->second.type == FileWatchEventType::Created && type == FileWatchEventType::Modified) {
      it->second.last_event_time = std::chrono::steady_clock::now();
      return;
    }
    it->second = {type, std::chrono::steady_clock::now()};
  } else {
    pending_events[std::move(path)] = {type, std::chrono::steady_clock::now()};
  }
}

} // mole
