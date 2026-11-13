#pragma once

#include "node_system/node_types.hpp"
#include "utils/uuid.hpp"
#include "utils/result.hpp"

namespace mole {

class INodeResolver {
public:
    virtual ~INodeResolver() = default;
    
    virtual Result<UUID> get_uuid(NodeID node_id) const = 0;
    virtual NodeID get_node_by_uuid(const UUID uuid) const = 0;
};

} // mole