#pragma once
#include "helpers.hpp"
#include "params.hpp"
#include <array>
#include <concepts>
#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace ecs {

class ISystem {
public:
  virtual ~ISystem() = default;
  virtual void run(World &world) = 0;
  virtual std::string name() const = 0;
  virtual std::string type_name() const { return name(); }
  virtual SystemAccess access() const { return {}; }

  Tick last_run() const { return last_run_; }
  void set_last_run(Tick t) { last_run_ = t; }

protected:
  Tick last_run_{};
};

namespace detail {

template <typename... Args> struct function_traits_impl {
  template <typename Fn> static decltype(auto) apply(Fn &&fn) {
    return std::forward<Fn>(fn).template operator()<Args...>();
  }
};

} // namespace detail

template <typename T>
struct function_traits
    : function_traits<decltype(&std::decay_t<T>::operator())> {};

template <typename C, typename R, typename... Args>
struct function_traits<R (C::*)(Args...) noexcept>
    : detail::function_traits_impl<Args...> {};

template <typename C, typename R, typename... Args>
struct function_traits<R (C::*)(Args...)>
    : detail::function_traits_impl<Args...> {};

template <typename C, typename R, typename... Args>
struct function_traits<R (C::*)(Args...) const noexcept>
    : detail::function_traits_impl<Args...> {};

template <typename C, typename R, typename... Args>
struct function_traits<R (C::*)(Args...) const>
    : detail::function_traits_impl<Args...> {};

template <typename R, typename... Args>
struct function_traits<R (*)(Args...) noexcept>
    : detail::function_traits_impl<Args...> {};

template <typename R, typename... Args>
struct function_traits<R (*)(Args...)> : detail::function_traits_impl<Args...> {
};

template <typename T>
concept SystemParameter =
    requires(World &w, const SystemContext &ctx, SystemAccess &acc) {
      SystemParamTraits<bare_t<T>>::retrieve(w, ctx);
      SystemParamTraits<bare_t<T>>::access(acc);
    };

template <typename F> class FunctionSystem : public ISystem {
  F func_;
  std::string name_;
  std::string type_name_;

public:
  explicit FunctionSystem(F func, std::string name = "")
      : func_(std::move(func)), type_name_(demangle(typeid(F).name())) {
    if (!name.empty()) {
      name_ = std::move(name);
    } else {
      name_ = system_name_of(func_);
    }
  }

  void run(World &world) override {
    Tick this_run = world.read_change_tick();
    SystemContext ctx{last_run_, this_run};
    function_traits<F>::apply(
        [&]<SystemParameter... Args>() { run_with_args<Args...>(world, ctx); });
    last_run_ = this_run;
  }

  std::string name() const override { return name_; }
  std::string type_name() const override { return type_name_; }

  SystemAccess access() const override {
    SystemAccess acc;
    function_traits<F>::apply([&]<SystemParameter... Args>() {
      access_with_args<Args...>(acc, type_name_);
    });
    return acc;
  }

private:
  template <SystemParameter... Args>
  void run_with_args(World &world, const SystemContext &ctx) {
    func_(SystemParamTraits<bare_t<Args>>::retrieve(world, ctx)...);
  }

  template <SystemParameter... Args>
  static void access_with_args(SystemAccess &acc,
                               const std::string &system_name) {
    std::array<SystemAccess, sizeof...(Args)> per_param;
    std::size_t i = 0;
    ((SystemParamTraits<bare_t<Args>>::access(per_param[i]), ++i), ...);

    for (std::size_t a = 0; a < per_param.size(); ++a) {
      if (per_param[a].is_empty())
        continue;
      for (std::size_t b = a + 1; b < per_param.size(); ++b) {
        if (per_param[b].is_empty())
          continue;
        if (!per_param[a].is_compatible(per_param[b]) &&
            !per_param[a].is_archetype_disjoint(per_param[b])) {
          throw std::runtime_error(
              "System '" + system_name +
              "': parameter conflict detected between parameters " +
              std::to_string(a) + " and " + std::to_string(b) +
              " (conflicting access to the same component or resource)");
        }
      }
    }

    for (auto &p : per_param) {
      acc.merge(p);
    }
  }
};

template <typename F>
std::unique_ptr<ISystem> make_system(F &&func, std::string name = "") {
  return std::make_unique<FunctionSystem<decay_t<F>>>(std::forward<F>(func),
                                                      std::move(name));
}

template <typename Sched, typename F>
void add_single_system(Sched &schedule, F &&func) {
  schedule.add_system(make_system(std::forward<F>(func)));
}

} // namespace ecs
