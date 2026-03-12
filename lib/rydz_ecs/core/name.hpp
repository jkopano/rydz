#pragma once
#include <string>

namespace ecs {

struct Name {
  std::string value;

  Name() = default;
  explicit Name(std::string name) : value(std::move(name)) {}

  const char *c_str() const { return value.c_str(); }
  bool empty() const { return value.empty(); }
};

} // namespace ecs
