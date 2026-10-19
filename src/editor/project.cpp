#include "editor/project.hpp"

namespace mole {

Result<std::unique_ptr<Project>> Project::create(const ProjectOptions& options) {
  std::unique_ptr<Project> project(new Project());

  if (!std::filesystem::is_directory(options.location)) {
    return mole::fail("Project location is not a directory");
  }

  project->name = options.name;
  project->location = options.location;

  return std::move(project);
}

} // mole