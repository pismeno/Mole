#pragma once

#include "utils/id.hpp"

namespace mole {

/**
 * @brief Unique identifier for a material. It consists of a 32-bit index and a 32-bit generation-counter.
 */
using MaterialID = ID<struct MaterialIDTag>;

} // mole