#pragma once

namespace mole {

struct NodeSystemContext;
struct RenderingContext;
struct OSContext;
struct ResourcesContext;

class ProcessOrchestrator;
class IImGuiBridge;

/**
 * @brief Context for the editor Application layer.
 */
struct EditorContext {
  NodeSystemContext* node_system_context;
  RenderingContext* rendering_context;
  OSContext* os_context;
  ResourcesContext* resources_context;

  ProcessOrchestrator* process_orchestrator;
  IImGuiBridge* im_gui_bridge;
};

} // mole