#pragma once
#include <string>

namespace ecs {

struct Name {
  std::string value;

  Name() = default;
  explicit Name(std::string name) : value(std::move(name)) {}

  auto c_str() const -> const char * { return value.c_str(); }
  auto empty() const -> bool { return value.empty(); }
};

} // namespace ecs
