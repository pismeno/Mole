#include "editor/project.hpp"

namespace mole {

Result<std::unique_ptr<Project>> Project::create(const ProjectOptions& options) {
  std::unique_ptr<Project> project(new Project());
  project->name = options.name;
  return std::move(project);
}

} // mole