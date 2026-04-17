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
  ISystem() = default;
  ISystem(const ISystem &) = default;
  ISystem(ISystem &&) = delete;
  auto operator=(const ISystem &) -> ISystem & = default;
  auto operator=(ISystem &&) -> ISystem & = delete;
  virtual ~ISystem() = default;
  virtual auto run(World &world) -> void = 0;
  [[nodiscard]] virtual auto name() const -> std::string = 0;
  [[nodiscard]] virtual auto type_name() const -> std::string { return name(); }
  [[nodiscard]] virtual auto access() const -> SystemAccess { return {}; }

  [[nodiscard]] auto last_run() const -> Tick { return last_run_; }
  auto set_last_run(Tick tick) -> void { last_run_ = tick; }

protected:
  Tick last_run_{};
};

namespace detail {

template <typename... Args> struct function_traits_impl {
  using args_tuple = Tuple<Args...>;

  template <typename Fn> static auto apply(Fn &&fn) -> decltype(auto) {
    return std::forward<Fn>(fn).template operator()<Args...>();
  }
};

template <typename> inline constexpr bool always_false_v = false;

} // namespace detail

template <typename T, typename = void> struct function_traits;

template <typename T>
struct function_traits<T, std::void_t<decltype(&std::decay_t<T>::operator())>>
    : function_traits<decltype(&std::decay_t<T>::operator())> {};

template <typename C, typename R, typename... Args>
struct function_traits<R (C::*)(Args...) noexcept, void>
    : detail::function_traits_impl<Args...> {};

template <typename C, typename R, typename... Args>
struct function_traits<R (C::*)(Args...), void>
    : detail::function_traits_impl<Args...> {};

template <typename C, typename R, typename... Args>
struct function_traits<R (C::*)(Args...) const noexcept, void>
    : detail::function_traits_impl<Args...> {};

template <typename C, typename R, typename... Args>
struct function_traits<R (C::*)(Args...) const, void>
    : detail::function_traits_impl<Args...> {};

template <typename R, typename... Args>
struct function_traits<R (*)(Args...) noexcept, void>
    : detail::function_traits_impl<Args...> {};

template <typename R, typename... Args>
struct function_traits<R (*)(Args...), void>
    : detail::function_traits_impl<Args...> {};

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

namespace detail {

template <typename TupleT> struct all_system_parameters;

template <typename... Args>
struct all_system_parameters<Tuple<Args...>>
    : std::bool_constant<(SystemParameter<Args> && ...)> {};

template <typename F, typename = void>
struct is_system_callable : std::false_type {};

template <typename F>
struct is_system_callable<
    F, std::void_t<typename function_traits<decay_t<F>>::args_tuple>>
    : all_system_parameters<typename function_traits<decay_t<F>>::args_tuple> {
};

template <typename T> struct is_system_input_descriptor : std::false_type {};

template <typename F> constexpr void static_assert_valid_system_callable() {
  static_assert(
      always_false_v<F>,
      "ECS system must be a non-generic callable whose every argument is a "
      "valid SystemParameter (for example World&, Res<T>, ResMut<T>, "
      "Query<...>, Commands, Local<T>, MessageReader<T>, or "
      "MessageWriter<T>).");
}

} // namespace detail

template <typename F>
concept SystemCallable = detail::is_system_callable<F>::value;

template <typename F>
concept SystemInput =
    SystemCallable<F> || detail::is_system_input_descriptor<bare_t<F>>::value;

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

  auto run(World &world) -> void override {
    ensure_param_states(world);

    Tick this_run = world.read_change_tick();
    SystemContext ctx{last_run_, this_run};
    function_traits<F>::apply([&]<SystemParameter... Args>() -> auto {
      run_with_args<Args...>(world, ctx);
    });
    last_run_ = this_run;
  }

  [[nodiscard]] auto name() const -> std::string override { return name_; }
  [[nodiscard]] auto type_name() const -> std::string override {
    return type_name_;
  }

  [[nodiscard]] auto access() const -> SystemAccess override {
    SystemAccess acc;
    function_traits<F>::apply([&]<SystemParameter... Args>() -> auto {
      access_with_args<Args...>(acc, type_name_);
    });
    return acc;
  }

private:
  auto ensure_param_states(World &world) -> void {
    if (state_world_ == &world) {
      return;
    }

    function_traits<F>::apply([&]<SystemParameter... Args>() -> auto {
      param_states_ = Tuple<typename SystemParamTraits<bare_t<Args>>::State...>{
          SystemParamTraits<bare_t<Args>>::init_state(world)...};
    });
    state_world_ = &world;
  }

  template <SystemParameter Arg>
  static auto
  retrieve_param(World &world, const SystemContext &ctx,
                 typename SystemParamTraits<bare_t<Arg>>::State &state)
      -> decltype(auto) {
    if constexpr (HasStatefulParamRetrieve<Arg>) {
      return SystemParamTraits<bare_t<Arg>>::retrieve(world, ctx, state);
    } else {
      return SystemParamTraits<bare_t<Arg>>::retrieve(world, ctx);
    }
  }

  template <SystemParameter... Args>
  auto run_with_args(World &world, const SystemContext &ctx) -> void {
    run_with_args_impl<Args...>(world, ctx, std::index_sequence_for<Args...>{});
  }

  template <SystemParameter... Args, std::size_t... I>
  auto run_with_args_impl(World &world, const SystemContext &ctx,
                          std::index_sequence<I...>) -> void {
    func_(retrieve_param<Args>(world, ctx, std::get<I>(param_states_))...);
  }

  template <SystemParameter... Args>
  static auto access_with_args(SystemAccess &acc,
                               const std::string &system_name) -> void {
    std::array<SystemAccess, sizeof...(Args)> per_param;
    std::size_t idx = 0;
    ((SystemParamTraits<bare_t<Args>>::access(per_param[idx]), ++idx), ...);

    for (std::size_t a = 0; a < per_param.size(); ++a) {
      if (!per_param[a].has_data_access()) {
        continue;
      }
      for (std::size_t b = a + 1; b < per_param.size(); ++b) {
        if (!per_param[b].has_data_access()) {
          continue;
        }
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
  requires SystemCallable<F>
auto make_system(F &&func, std::string name = "") -> std::unique_ptr<ISystem> {
  return std::make_unique<FunctionSystem<decay_t<F>>>(std::forward<F>(func),
                                                      std::move(name));
}

template <typename F>
  requires(!SystemCallable<F>)
auto make_system(F && /*func*/, std::string /*name*/ = "")
    -> std::unique_ptr<ISystem> {
  detail::static_assert_valid_system_callable<F>();
  return nullptr;
}

template <typename Sched, typename F>
auto add_single_system(Sched &schedule, F &&func) -> void {
  schedule.add_system(make_system(std::forward<F>(func)));
}

} // namespace ecs
