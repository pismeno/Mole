#include "editor/project_manager.hpp"

namespace mole {

Result<void> ProjectManager::create_project(const ProjectOptions& options) {
  file_system->mount("project", "../../../" + options.name);
  file_system->mount("assets", "../../../" + options.name + "/cache/");

  auto r_create = Project::create(options);
  if (!r_create) return mole::fail(r_create.error());
  active_project = std::move(r_create).value();

  return mole::ok();
}

} // mole