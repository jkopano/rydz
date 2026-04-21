#pragma once

#include <cstddef>

namespace rydz {

inline void hash_combine(std::size_t &seed, std::size_t value) noexcept {
  seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
}

} // namespace rydz
