#pragma once
#include "bundle.hpp"
#include "helpers.hpp"
#include "params.hpp"
#include <algorithm>
#include <concepts>
#include <memory>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ecs {

template <typename E> struct On {
  const E *event{};
  Entity observer{};
  std::optional<Entity> target;

  auto operator*() const -> const E & { return *event; }
  auto operator->() const -> const E * { return event; }

  [[nodiscard]] auto observer_entity() const -> Entity { return observer; }
  [[nodiscard]] auto target_entity() const -> std::optional<Entity> {
    return target;
  }
};

struct ObservedBy {
  std::vector<Entity> observers;
};

class IObserver {
public:
  IObserver() = default;
  IObserver(const IObserver &) = default;
  IObserver(IObserver &&) = delete;
  auto operator=(const IObserver &) -> IObserver & = default;
  auto operator=(IObserver &&) -> IObserver & = delete;
  virtual ~IObserver() = default;
  virtual auto run(World &world, const void *event, Entity observer,
                   std::optional<Entity> target) -> void = 0;
};

namespace detail {

template <typename... Args> struct observer_function_traits_impl {
  using args_tuple = Tuple<Args...>;

  template <typename Fn> static auto apply(Fn &&fn) -> decltype(auto) {
    return std::forward<Fn>(fn).template operator()<Args...>();
  }
};

template <typename T>
struct observer_function_traits
    : observer_function_traits<decltype(&std::decay_t<T>::operator())> {};

template <typename C, typename R, typename... Args>
struct observer_function_traits<R (C::*)(Args...) noexcept>
    : observer_function_traits_impl<Args...> {};

template <typename C, typename R, typename... Args>
struct observer_function_traits<R (C::*)(Args...)>
    : observer_function_traits_impl<Args...> {};

template <typename C, typename R, typename... Args>
struct observer_function_traits<R (C::*)(Args...) const noexcept>
    : observer_function_traits_impl<Args...> {};

template <typename C, typename R, typename... Args>
struct observer_function_traits<R (C::*)(Args...) const>
    : observer_function_traits_impl<Args...> {};

template <typename R, typename... Args>
struct observer_function_traits<R (*)(Args...) noexcept>
    : observer_function_traits_impl<Args...> {};

template <typename R, typename... Args>
struct observer_function_traits<R (*)(Args...)>
    : observer_function_traits_impl<Args...> {};

template <typename T>
concept HasStatefulObserverParamRetrieve =
    requires(World &w, const SystemContext &ctx,
             typename SystemParamTraits<bare_t<T>>::State &state) {
      SystemParamTraits<bare_t<T>>::retrieve(w, ctx, state);
    };

template <typename T>
concept HasStatelessObserverParamRetrieve =
    requires(World &w, const SystemContext &ctx) {
      SystemParamTraits<bare_t<T>>::retrieve(w, ctx);
    };

template <typename T>
concept ObserverSystemParameter =
    requires(World &w, SystemAccess &acc) {
      typename SystemParamTraits<bare_t<T>>::State;
      {
        SystemParamTraits<bare_t<T>>::init_state(w)
      } -> std::same_as<typename SystemParamTraits<bare_t<T>>::State>;
      SystemParamTraits<bare_t<T>>::access(acc);
    } && (HasStatefulObserverParamRetrieve<T> ||
          HasStatelessObserverParamRetrieve<T>);

template <typename T> struct is_on_param : std::false_type {};
template <typename E> struct is_on_param<On<E>> : std::true_type {};

template <typename T>
inline constexpr bool is_on_param_v = is_on_param<bare_t<T>>::value;

template <typename T> struct on_event_type;
template <typename E> struct on_event_type<On<E>> {
  using type = E;
};

template <typename... Args> struct first_on_type_impl;
template <> struct first_on_type_impl<> {
  using type = void;
};

template <bool IsOn, typename First, typename... Rest>
struct first_on_type_selector;

template <typename First, typename... Rest>
struct first_on_type_selector<true, First, Rest...> {
  using type = typename on_event_type<bare_t<First>>::type;
};

template <typename First, typename... Rest>
struct first_on_type_selector<false, First, Rest...> {
  using type = typename first_on_type_impl<Rest...>::type;
};

template <typename First, typename... Rest>
struct first_on_type_impl<First, Rest...> {
  using type = typename first_on_type_selector<is_on_param_v<First>, First,
                                               Rest...>::type;
};

template <typename TupleT> struct observer_traits;
template <typename... Args> struct observer_traits<Tuple<Args...>> {
  static constexpr usize on_count = (0 + ... + (is_on_param_v<Args> ? 1 : 0));
  using event_type = typename first_on_type_impl<Args...>::type;
};

template <typename F>
using observer_event_t = typename observer_traits<
    typename observer_function_traits<decay_t<F>>::args_tuple>::event_type;

template <typename F>
inline constexpr usize observer_on_count_v = observer_traits<
    typename observer_function_traits<decay_t<F>>::args_tuple>::on_count;

template <typename T>
using observer_state_t =
    std::conditional_t<is_on_param_v<T>, std::monostate,
                       typename SystemParamTraits<bare_t<T>>::State>;

template <typename T>
concept ObserverParameter = is_on_param_v<T> || ObserverSystemParameter<T>;

template <typename E> auto entity_event_target(const E &event) -> Entity {
  auto tuple = to_tuple(event);
  static_assert(std::tuple_size_v<decltype(tuple)> > 0,
                "EntityEvent must have Entity as its first field.");
  using First = bare_t<std::tuple_element_t<0, decltype(tuple)>>;
  static_assert(std::same_as<First, Entity>,
                "EntityEvent must have Entity as its first field.");
  return std::get<0>(tuple);
}

template <typename F> class FunctionObserver final : public IObserver {
  using ParamTuple = typename observer_function_traits<F>::args_tuple;
  using EventT = observer_event_t<F>;

  template <typename TupleT> struct ParamStateTuple;
  template <typename... Args> struct ParamStateTuple<Tuple<Args...>> {
    using type = Tuple<observer_state_t<Args>...>;
  };

  F func_;
  typename ParamStateTuple<ParamTuple>::type param_states_{};
  World *state_world_ = nullptr;
  Tick last_run_{};

public:
  explicit FunctionObserver(F func) : func_(std::move(func)) {
    static_assert(observer_on_count_v<F> == 1,
                  "Observer callback must have exactly one On<E> parameter.");
    static_assert(IsEvent<EventT>,
                  "Observer On<E> parameter must use an Event or EntityEvent.");
  }

  auto run(World &world, const void *event, Entity observer,
           std::optional<Entity> target) -> void override {
    ensure_param_states(world);

    Tick this_run = world.read_change_tick();
    SystemContext ctx{.last_run = last_run_, .this_run = this_run};
    const auto &typed_event = *static_cast<const EventT *>(event);
    observer_function_traits<F>::apply(
        [&]<ObserverParameter... Args>() -> auto {
          run_with_args<Args...>(world, ctx, typed_event, observer, target);
        });
    last_run_ = this_run;
  }

private:
  template <typename Arg>
  static auto init_param_state(World &world) -> observer_state_t<Arg> {
    if constexpr (is_on_param_v<Arg>) {
      return {};
    } else {
      return SystemParamTraits<bare_t<Arg>>::init_state(world);
    }
  }

  auto ensure_param_states(World &world) -> void {
    if (state_world_ == &world) {
      return;
    }

    observer_function_traits<F>::apply(
        [&]<ObserverParameter... Args>() -> auto {
          param_states_ = Tuple<observer_state_t<Args>...>{
              init_param_state<Args>(world)...};
        });
    state_world_ = &world;
  }

  template <ObserverParameter Arg>
  static auto retrieve_param(World &world, const SystemContext &ctx,
                             observer_state_t<Arg> &state, const EventT &event,
                             Entity observer, std::optional<Entity> target)
      -> decltype(auto) {
    using Raw = bare_t<Arg>;

    if constexpr (is_on_param_v<Raw>) {
      return Raw{&event, observer, target};
    } else if constexpr (HasStatefulObserverParamRetrieve<Arg>) {
      return SystemParamTraits<Raw>::retrieve(world, ctx, state);
    } else {
      return SystemParamTraits<Raw>::retrieve(world, ctx);
    }
  }

  template <ObserverParameter... Args>
  auto run_with_args(World &world, const SystemContext &ctx,
                     const EventT &event, Entity observer,
                     std::optional<Entity> target) -> void {
    run_with_args_impl<Args...>(world, ctx, event, observer, target,
                                std::index_sequence_for<Args...>{});
  }

  template <ObserverParameter... Args, std::size_t... I>
  auto run_with_args_impl(World &world, const SystemContext &ctx,
                          const EventT &event, Entity observer,
                          std::optional<Entity> target,
                          std::index_sequence<I...>) -> void {
    func_(retrieve_param<Args>(world, ctx, std::get<I>(param_states_), event,
                               observer, target)...);
  }
};

template <typename F>
auto make_observer(F &&func) -> std::unique_ptr<IObserver> {
  using Fn = decay_t<F>;
  return std::make_unique<FunctionObserver<Fn>>(std::forward<F>(func));
}

} // namespace detail

class ObserverRegistry {
public:
  using T = Resource;

private:
  struct ObserverEntry {
    std::type_index event_type = std::type_index(typeid(void));
    std::unique_ptr<IObserver> callback;
    std::optional<Entity> target;
  };

  std::unordered_set<std::type_index> registered_events_;
  std::unordered_map<Entity, ObserverEntry> observers_;
  std::unordered_map<std::type_index, std::vector<Entity>> global_observers_;
  std::unordered_map<std::type_index,
                     std::unordered_map<Entity, std::vector<Entity>>>
      entity_observers_;
  std::unordered_map<Entity, std::vector<Entity>> observers_by_target_;

  static auto erase_observer_id(std::vector<Entity> &items, Entity observer)
      -> void {
    std::erase(items, observer);
  }

  auto ensure_registered(std::type_index event_type) const -> void {
    if (!registered_events_.contains(event_type)) {
      throw std::runtime_error("Reactive event type was not registered via "
                               "add_event<T>().");
    }
  }

public:
  template <typename E>
    requires IsEvent<bare_t<E>>
  auto register_event() -> void {
    registered_events_.insert(std::type_index(typeid(bare_t<E>)));
  }

  template <typename E>
    requires IsEvent<bare_t<E>>
  auto has_event() const -> bool {
    return registered_events_.contains(std::type_index(typeid(bare_t<E>)));
  }

  auto has_event(std::type_index event_type) const -> bool {
    return registered_events_.contains(event_type);
  }

  auto has_observer(Entity observer) const -> bool {
    return observers_.contains(observer);
  }

  auto observers_for_target(Entity target) const -> std::vector<Entity> {
    auto iter = observers_by_target_.find(target);
    return (iter == observers_by_target_.end()) ? std::vector<Entity>{}
                                                : iter->second;
  }

  auto insert_global(Entity observer, std::type_index event_type,
                     std::unique_ptr<IObserver> callback) -> void {
    ensure_registered(event_type);
    observers_[observer] = ObserverEntry{.event_type = event_type,
                                         .callback = std::move(callback),
                                         .target = {}};
    global_observers_[event_type].push_back(observer);
  }

  auto insert_targeted(World &world, Entity observer, Entity target,
                       std::type_index event_type,
                       std::unique_ptr<IObserver> callback) -> void {
    ensure_registered(event_type);
    observers_[observer] = ObserverEntry{.event_type = event_type,
                                         .callback = std::move(callback),
                                         .target = target};
    entity_observers_[event_type][target].push_back(observer);
    observers_by_target_[target].push_back(observer);

    auto *observed_by = world.get_component<ObservedBy>(target);
    if (observed_by == nullptr) {
      world.insert_component(target, ObservedBy{});
      observed_by = world.get_component<ObservedBy>(target);
    }
    observed_by->observers.push_back(observer);
  }

  auto remove_observer(World &world, Entity observer) -> void {
    auto it = observers_.find(observer);
    if (it == observers_.end()) {
      return;
    }

    auto event_type = it->second.event_type;
    auto target = it->second.target;

    if (target) {
      if (auto by_type = entity_observers_.find(event_type);
          by_type != entity_observers_.end()) {
        if (auto by_target = by_type->second.find(*target);
            by_target != by_type->second.end()) {
          erase_observer_id(by_target->second, observer);
          if (by_target->second.empty()) {
            by_type->second.erase(by_target);
          }
        }
        if (by_type->second.empty()) {
          entity_observers_.erase(by_type);
        }
      }

      if (auto reverse = observers_by_target_.find(*target);
          reverse != observers_by_target_.end()) {
        erase_observer_id(reverse->second, observer);
        if (reverse->second.empty()) {
          observers_by_target_.erase(reverse);
        }
      }

      if (auto *observed_by = world.get_component<ObservedBy>(*target)) {
        erase_observer_id(observed_by->observers, observer);
        if (observed_by->observers.empty()) {
          world.remove_component<ObservedBy>(*target);
        }
      }
    } else if (auto global = global_observers_.find(event_type);
               global != global_observers_.end()) {
      erase_observer_id(global->second, observer);
      if (global->second.empty()) {
        global_observers_.erase(global);
      }
    }

    observers_.erase(it);
  }

  template <typename E>
    requires IsEvent<bare_t<E>>
  auto trigger(World &world, E &event) -> void {
    using EventT = bare_t<E>;
    auto event_type = std::type_index(typeid(EventT));
    ensure_registered(event_type);

    auto run_ids = [&](const std::vector<Entity> &ids,
                       std::optional<Entity> target) -> auto {
      auto pending = ids;
      for (Entity observer : pending) {
        auto iter = observers_.find(observer);
        if (iter == observers_.end()) {
          continue;
        }
        iter->second.callback->run(world, &event, observer, target);
      }
    };

    if (auto global = global_observers_.find(event_type);
        global != global_observers_.end()) {
      run_ids(global->second, std::nullopt);
    }

    if constexpr (IsEntityEvent<EventT>) {
      Entity target = detail::entity_event_target(event);
      if (auto by_type = entity_observers_.find(event_type);
          by_type != entity_observers_.end()) {
        if (auto targeted = by_type->second.find(target);
            targeted != by_type->second.end()) {
          run_ids(targeted->second, target);
        }
      }
    }
  }
};

inline auto World::ensure_observer_registry() -> ObserverRegistry & {
  auto *registry = get_resource<ObserverRegistry>();
  if (registry == nullptr) {
    insert_resource(ObserverRegistry{});
    registry = get_resource<ObserverRegistry>();
  }
  return *registry;
}

inline auto World::observer_registry() const -> const ObserverRegistry * {
  return get_resource<ObserverRegistry>();
}

inline auto World::despawn_immediate(Entity entity) -> void {
  for (auto &[_, storage] : storages_) {
    storage->remove(entity);
  }
  entities.destroy(entity);
}

inline auto World::despawn(Entity entity) -> void {
  std::vector<Entity> attached_observers;
  bool is_observer = false;

  if (const auto *registry = observer_registry()) {
    attached_observers = registry->observers_for_target(entity);
    is_observer = registry->has_observer(entity);
  }

  auto &registry = ensure_observer_registry();
  for (Entity observer : attached_observers) {
    registry.remove_observer(*this, observer);
    despawn_immediate(observer);
  }

  if (is_observer) {
    registry.remove_observer(*this, entity);
  }

  despawn_immediate(entity);
}

template <typename E>
  requires IsEvent<bare_t<E>>
inline auto World::add_event() -> void {
  ensure_observer_registry().template register_event<bare_t<E>>();
}

template <typename F> inline auto World::add_observer(F &&func) -> Entity {
  using EventT = detail::observer_event_t<F>;

  Entity observer = spawn();
  ensure_observer_registry().insert_global(
      observer, std::type_index(typeid(EventT)),
      detail::make_observer(std::forward<F>(func)));
  return observer;
}

template <typename F>
inline auto World::add_observer(Entity target, F &&func) -> Entity {
  using EventT = detail::observer_event_t<F>;
  static_assert(IsEntityEvent<EventT>,
                "Entity-targeted observers require On<E> where E is an "
                "EntityEvent.");

  if (!entities.is_alive(target)) {
    throw std::runtime_error("Cannot add an observer to a dead entity.");
  }

  Entity observer = spawn();
  ensure_observer_registry().insert_targeted(
      *this, observer, target, std::type_index(typeid(EventT)),
      detail::make_observer(std::forward<F>(func)));
  return observer;
}

template <typename E>
  requires IsEvent<bare_t<E>>
inline auto World::trigger(E &&event) -> void {
  if constexpr (std::is_lvalue_reference_v<E &&>) {
    ensure_observer_registry().trigger(*this, event);
  } else {
    bare_t<E> local_event = std::forward<E>(event);
    ensure_observer_registry().trigger(*this, local_event);
  }
}

} // namespace ecs
