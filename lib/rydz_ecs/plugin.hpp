#pragma once
#include <functional>

namespace ecs {

class App;

class IPlugin {
public:
  IPlugin() = default;
  IPlugin(const IPlugin &) = default;
  IPlugin(IPlugin &&) = delete;
  auto operator=(const IPlugin &) -> IPlugin & = default;
  auto operator=(IPlugin &&) -> IPlugin & = delete;
  virtual ~IPlugin() = default;
  virtual auto build(App &app) -> void = 0;
};

class FunctionPlugin : public IPlugin {
  std::function<void(App &)> func_;

public:
  explicit FunctionPlugin(std::function<void(App &)> func)
      : func_(std::move(func)) {}

  auto build(App &app) -> void override { func_(app); }
};

} // namespace ecs
