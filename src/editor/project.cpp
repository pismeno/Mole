#include "editor/project.hpp"

namespace mole {

Result<std::unique_ptr<Project>> Project::create(const ProjectOptions& options) {
  std::unique_ptr<Project> project(new Project());
  project->name = options.name;
  auto r_create_file_watcher = FileWatcher::create("project://assets/", options.context);
  if (!r_create_file_watcher) return mole::fail(r_create_file_watcher.error());
  project->assets_file_watcher = std::move(r_create_file_watcher).value();

  return std::move(project);
}

} // mole