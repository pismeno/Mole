#include "editor/file_watcher.hpp"
#include "osal/ifile_system.hpp"
#include "osal/os_context.hpp"

namespace mole {

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
  std::vector<FileWatchEvent> events_to_process = std::move(events);
  events.clear();
  return events_to_process;
}

void FileWatcher::push_event(std::filesystem::path path, FileWatchEventType type) {
  std::lock_guard<std::mutex> lock(mutex);
  events.push_back({std::move(path), type});
}

} // mole