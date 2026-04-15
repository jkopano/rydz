#pragma once
#include <functional>

namespace ecs {

class App;

class IPlugin {
public:
  IPlugin() = default;
  IPlugin(const IPlugin &) = default;
  IPlugin(IPlugin &&) = delete;
  IPlugin &operator=(const IPlugin &) = default;
  IPlugin &operator=(IPlugin &&) = delete;
  virtual ~IPlugin() = default;
  virtual void build(App &app) = 0;
};

class FunctionPlugin : public IPlugin {
  std::function<void(App &)> func_;

public:
  explicit FunctionPlugin(std::function<void(App &)> func)
      : func_(std::move(func)) {}

  void build(App &app) override { func_(app); }
};

} // namespace ecs
