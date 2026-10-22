#pragma once

#include <cstddef>
#include <vector>

namespace mole {

struct PackedMaterial {
  std::vector<std::byte> data;
};

} // mole