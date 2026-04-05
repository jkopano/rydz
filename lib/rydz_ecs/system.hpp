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
  using args_tuple = Tuple<Args...>;

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
concept HasStatefulParamRetrieve =
    requires(World &w, const SystemContext &ctx,
             typename SystemParamTraits<bare_t<T>>::State &state) {
      SystemParamTraits<bare_t<T>>::retrieve(w, ctx, state);
    };

template <typename T>
concept HasStatelessParamRetrieve =
    requires(World &w, const SystemContext &ctx) {
      SystemParamTraits<bare_t<T>>::retrieve(w, ctx);
    };

template <typename T>
concept SystemParameter = requires(World &w, SystemAccess &acc) {
  typename SystemParamTraits<bare_t<T>>::State;
  {
    SystemParamTraits<bare_t<T>>::init_state(w)
  } -> std::same_as<typename SystemParamTraits<bare_t<T>>::State>;
  SystemParamTraits<bare_t<T>>::access(acc);
} && (HasStatefulParamRetrieve<T> || HasStatelessParamRetrieve<T>);

template <typename F> class FunctionSystem : public ISystem {
  F func_;
  std::string name_;
  std::string type_name_;
  using ParamTuple = typename function_traits<F>::args_tuple;

  template <typename TupleT> struct ParamStateTuple;
  template <typename... Args> struct ParamStateTuple<Tuple<Args...>> {
    using type = Tuple<typename SystemParamTraits<bare_t<Args>>::State...>;
  };

  typename ParamStateTuple<ParamTuple>::type param_states_{};
  World *state_world_ = nullptr;

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
    ensure_param_states(world);

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
  void ensure_param_states(World &world) {
    if (state_world_ == &world)
      return;

    function_traits<F>::apply([&]<SystemParameter... Args>() {
      param_states_ = Tuple<typename SystemParamTraits<bare_t<Args>>::State...>{
          SystemParamTraits<bare_t<Args>>::init_state(world)...};
    });
    state_world_ = &world;
  }

  template <SystemParameter Arg>
  static decltype(auto)
  retrieve_param(World &world, const SystemContext &ctx,
                 typename SystemParamTraits<bare_t<Arg>>::State &state) {
    if constexpr (HasStatefulParamRetrieve<Arg>) {
      return SystemParamTraits<bare_t<Arg>>::retrieve(world, ctx, state);
    } else {
      return SystemParamTraits<bare_t<Arg>>::retrieve(world, ctx);
    }
  }

  template <SystemParameter... Args>
  void run_with_args(World &world, const SystemContext &ctx) {
    run_with_args_impl<Args...>(world, ctx, std::index_sequence_for<Args...>{});
  }

  template <SystemParameter... Args, std::size_t... I>
  void run_with_args_impl(World &world, const SystemContext &ctx,
                          std::index_sequence<I...>) {
    func_(retrieve_param<Args>(world, ctx, std::get<I>(param_states_))...);
  }

  template <SystemParameter... Args>
  static void access_with_args(SystemAccess &acc,
                               const std::string &system_name) {
    std::array<SystemAccess, sizeof...(Args)> per_param;
    std::size_t i = 0;
    ((SystemParamTraits<bare_t<Args>>::access(per_param[i]), ++i), ...);

    for (std::size_t a = 0; a < per_param.size(); ++a) {
      if (!per_param[a].has_data_access())
        continue;
      for (std::size_t b = a + 1; b < per_param.size(); ++b) {
        if (!per_param[b].has_data_access())
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
