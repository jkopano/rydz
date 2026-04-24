#pragma once
#include <concepts>
#include <functional>

namespace ecs {

class App;

template <typename T>
concept PluginTrait = std::derived_from<T, IPlugin> && requires(T t, App& app) {
  { t.build(app) } -> std::same_as<void>;
};

class IPlugin {};

class FunctionPlugin : public IPlugin {
  std::function<void(App&)> func_;

public:
  explicit FunctionPlugin(std::function<void(App&)> func) : func_(std::move(func)) {}

  auto build(App& app) -> void { func_(app); }
};

} // namespace ecs
