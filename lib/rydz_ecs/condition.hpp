#pragma once
#include "fwd.hpp"
#include "system.hpp"
#include <algorithm>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <typeindex>
#include <vector>

namespace ecs {

namespace detail {

template <typename T, typename = void>
struct is_set_marker : std::bool_constant<std::is_base_of_v<Set, bare_t<T>>> {};

template <typename T>
struct is_set_marker<T, std::void_t<typename bare_t<T>::T>>
    : std::bool_constant<std::is_same_v<typename bare_t<T>::T, Set> ||
                         std::is_base_of_v<Set, bare_t<T>>> {};

template <typename T>
inline constexpr bool is_set_marker_v = is_set_marker<T>::value;

} // namespace detail

struct SetId {
  std::type_index type;
  i32 value;

  SetId(std::type_index t, i32 v) : type(t), value(v) {}

  bool operator==(const SetId &o) const {
    return type == o.type && value == o.value;
  }
};

inline std::string set_name(const SetId &id) {
  auto name = demangle(id.type.name());
  return name + "(" + std::to_string(id.value) + ")";
}

template <typename E>
  requires std::is_enum_v<E>
SetId set(E value) {
  return SetId{typeid(E), static_cast<i32>(value)};
}

template <typename S>
  requires detail::is_set_marker_v<S>
SetId set() {
  return SetId{typeid(S), 0};
}

struct SetList {
  std::vector<SetId> ids;
};

} // namespace ecs

template <> struct std::hash<ecs::SetId> {
  usize operator()(const ecs::SetId &s) const noexcept {
    usize h1 = std::hash<std::type_index>{}(s.type);
    usize h2 = std::hash<i32>{}(s.value);
    return h1 ^ (h2 * 2654435761u);
  }
};

namespace ecs {

template <typename H> H AbslHashValue(H h, const SetId &s) {
  return H::combine(std::move(h), s.type.hash_code(), s.value);
}

namespace detail {

template <typename T>
struct is_set_argument : std::bool_constant<std::is_enum_v<bare_t<T>>> {};
template <> struct is_set_argument<SetId> : std::true_type {};
template <typename T>
  requires is_set_marker_v<T>
struct is_set_argument<T> : std::true_type {};

template <typename T>
inline constexpr bool is_set_argument_v = is_set_argument<bare_t<T>>::value;

inline SetId to_set_id(SetId id) { return id; }

template <typename E>
  requires std::is_enum_v<bare_t<E>>
SetId to_set_id(E value) {
  return set(value);
}

template <typename S>
  requires is_set_marker_v<S>
SetId to_set_id(S &&) {
  return set<bare_t<S>>();
}

inline std::vector<SetId> to_set_ids(SetId id) { return {id}; }

inline std::vector<SetId> to_set_ids(SetList ids) { return std::move(ids.ids); }

inline std::string set_names(const std::vector<SetId> &ids) {
  std::string result;
  for (usize i = 0; i < ids.size(); ++i) {
    if (i != 0) {
      result += ", ";
    }
    result += set_name(ids[i]);
  }
  return result;
}

template <typename... Args> std::vector<SetId> make_set_ids(Args &&...args) {
  std::vector<SetId> ids;
  ids.reserve(sizeof...(Args));
  (ids.push_back(to_set_id(std::forward<Args>(args))), ...);
  return ids;
}

inline void ensure_unique_sets(const std::vector<SetId> &ids,
                               const std::string &context) {
  for (usize i = 0; i < ids.size(); ++i) {
    for (usize j = i + 1; j < ids.size(); ++j) {
      if (ids[i] == ids[j]) {
        throw std::runtime_error(context + ": duplicate set '" +
                                 set_name(ids[i]) + "'");
      }
    }
  }
}

} // namespace detail

struct SystemOrdering {
  std::vector<std::string> after;
  std::vector<std::string> before;
  std::vector<SetId> in_sets;
};

class ICondition {
public:
  ICondition() = default;
  ICondition(const ICondition &) = default;
  ICondition(ICondition &&) = delete;
  ICondition &operator=(const ICondition &) = default;
  ICondition &operator=(ICondition &&) = delete;
  virtual ~ICondition() = default;
  virtual bool is_true(World &world) = 0;
  [[nodiscard]] virtual SystemAccess access() const = 0;
};

template <typename F> class FunctionCondition : public ICondition {
  F func_;
  using ParamTuple = typename function_traits<F>::args_tuple;

  template <typename TupleT> struct ParamStateTuple;
  template <typename... Args> struct ParamStateTuple<Tuple<Args...>> {
    using type = Tuple<typename SystemParamTraits<bare_t<Args>>::State...>;
  };

  typename ParamStateTuple<ParamTuple>::type param_states_{};
  World *state_world_ = nullptr;

public:
  explicit FunctionCondition(F func) : func_(std::move(func)) {}

  bool is_true(World &world) override {
    ensure_param_states(world);

    SystemContext ctx{.last_run = Tick{0},
                      .this_run = world.read_change_tick()};
    return function_traits<F>::apply([&]<SystemParameter... Args>() {
      return evaluate_with_args<Args...>(world, ctx,
                                         std::index_sequence_for<Args...>{});
    });
  }

  [[nodiscard]] SystemAccess access() const override {
    SystemAccess acc;
    function_traits<F>::apply([&]<SystemParameter... Args>() {
      (SystemParamTraits<bare_t<Args>>::access(acc), ...);
    });
    return acc;
  }

private:
  void ensure_param_states(World &world) {
    if (state_world_ == &world) {
      return;
    }

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

  template <SystemParameter... Args, std::size_t... I>
  bool evaluate_with_args(World &world, const SystemContext &ctx,
                          std::index_sequence<I...>) {
    return func_(
        retrieve_param<Args>(world, ctx, std::get<I>(param_states_))...);
  }
};

class SharedConditionGate {
  std::shared_ptr<ICondition> condition_;
  usize member_count_;
  mutable std::mutex mutex_;
  u64 active_run_id_{};
  usize remaining_members_{};
  bool cached_result_ = false;

public:
  SharedConditionGate(std::shared_ptr<ICondition> condition, usize member_count)
      : condition_(std::move(condition)), member_count_(member_count) {}

  bool is_true(World &world) {
    std::lock_guard lock(mutex_);

    const u64 run_id = world.read_schedule_run_id();
    if (run_id != active_run_id_ || remaining_members_ == 0) {
      cached_result_ = condition_->is_true(world);
      active_run_id_ = run_id;
      remaining_members_ = member_count_;
    }

    if (remaining_members_ > 0) {
      remaining_members_--;
    }

    return cached_result_;
  }

  SystemAccess access() const { return condition_->access(); }
};

class ConditionedSystem : public ISystem {
  std::unique_ptr<ISystem> system_;
  std::shared_ptr<ICondition> condition_;
  std::shared_ptr<SharedConditionGate> shared_condition_;

public:
  ConditionedSystem(std::unique_ptr<ISystem> system,
                    std::unique_ptr<ICondition> condition)
      : system_(std::move(system)),
        condition_(std::shared_ptr<ICondition>(std::move(condition))) {}

  ConditionedSystem(std::unique_ptr<ISystem> system,
                    std::shared_ptr<ICondition> condition)
      : system_(std::move(system)), condition_(std::move(condition)) {}

  ConditionedSystem(std::unique_ptr<ISystem> system,
                    std::shared_ptr<SharedConditionGate> shared_condition)
      : system_(std::move(system)),
        shared_condition_(std::move(shared_condition)) {}

  void run(World &world) override {
    const bool should_run = shared_condition_
                                ? shared_condition_->is_true(world)
                                : condition_->is_true(world);

    if (should_run) {
      system_->run(world);
    }
  }

  [[nodiscard]] std::string name() const override { return system_->name(); }

  [[nodiscard]] SystemAccess access() const override {
    auto acc =
        shared_condition_ ? shared_condition_->access() : condition_->access();
    acc.merge(system_->access());
    return acc;
  }
};

template <typename F> std::unique_ptr<ICondition> make_condition(F &&func) {
  return std::make_unique<FunctionCondition<decay_t<F>>>(std::forward<F>(func));
}

inline std::unique_ptr<ICondition> run_once() {
  struct RunOnceCondition : ICondition {
    bool fired{};
    bool is_true(World & /*world*/) override {
      if (!fired) {
        fired = true;
        return true;
      }
      return false;
    }
    [[nodiscard]] SystemAccess access() const override { return {}; }
  };
  return std::make_unique<RunOnceCondition>();
}

template <typename F, typename Cond>
std::unique_ptr<ISystem> make_system_run_if(F &&func, Cond &&cond) {
  return std::make_unique<ConditionedSystem>(
      make_system(std::forward<F>(func)),
      make_condition(std::forward<Cond>(cond)));
}

template <typename F> class SystemDescriptor {
  F func_;
  std::unique_ptr<ICondition> condition_;
  SystemOrdering ordering_;

public:
  explicit SystemDescriptor(F func) : func_(std::move(func)) {}

  template <typename Cond> SystemDescriptor &&run_if(Cond &&cond) && {
    condition_ = make_condition(std::forward<Cond>(cond));
    return std::move(*this);
  }

  SystemDescriptor &&run_if(std::unique_ptr<ICondition> cond) && {
    condition_ = std::move(cond);
    return std::move(*this);
  }

  template <typename Fn> SystemDescriptor &&after(Fn &&fn) && {
    ordering_.after.push_back(system_name_of(std::forward<Fn>(fn)));
    return std::move(*this);
  }

  template <typename Fn> SystemDescriptor &&before(Fn &&fn) && {
    ordering_.before.push_back(system_name_of(std::forward<Fn>(fn)));
    return std::move(*this);
  }

  SystemOrdering take_ordering() { return std::move(ordering_); }

  std::unique_ptr<ISystem> build() {
    auto sys = make_system(std::move(func_));
    if (condition_) {
      return std::make_unique<ConditionedSystem>(std::move(sys),
                                                 std::move(condition_));
    }
    return sys;
  }
};

template <typename T> struct is_system_descriptor : std::false_type {};
template <typename F>
struct is_system_descriptor<SystemDescriptor<F>> : std::true_type {};
template <typename T>
inline constexpr bool is_system_descriptor_v = is_system_descriptor<T>::value;

struct GroupSystemEntry {
  std::unique_ptr<ISystem> system;
  SystemOrdering ordering;
};

class SystemGroupDescriptor {
  std::vector<std::unique_ptr<ISystem>> systems_;
  std::unique_ptr<ICondition> condition_;
  SystemOrdering ordering_;
  bool chained_ = false;

public:
  template <typename... Fs> explicit SystemGroupDescriptor(Fs &&...funcs) {
    (systems_.push_back(make_system(std::forward<Fs>(funcs))), ...);
  }

  template <typename Cond> SystemGroupDescriptor &&run_if(Cond &&cond) && {
    condition_ = make_condition(std::forward<Cond>(cond));
    return std::move(*this);
  }

  SystemGroupDescriptor &&run_if(std::unique_ptr<ICondition> cond) && {
    condition_ = std::move(cond);
    return std::move(*this);
  }

  template <typename Fn> SystemGroupDescriptor &&after(Fn &&fn) && {
    ordering_.after.push_back(system_name_of(std::forward<Fn>(fn)));
    return std::move(*this);
  }

  template <typename Fn> SystemGroupDescriptor &&before(Fn &&fn) && {
    ordering_.before.push_back(system_name_of(std::forward<Fn>(fn)));
    return std::move(*this);
  }

  SystemGroupDescriptor &&chain() && {
    chained_ = true;
    return std::move(*this);
  }

  std::vector<GroupSystemEntry> build() {
    std::vector<GroupSystemEntry> result;

    std::shared_ptr<ICondition> shared_cond = std::move(condition_);
    std::shared_ptr<SharedConditionGate> shared_gate;
    if (shared_cond) {
      shared_gate =
          std::make_shared<SharedConditionGate>(shared_cond, systems_.size());
    }

    std::string prev_name;
    for (auto &sys : systems_) {
      SystemOrdering entry_ordering = ordering_;

      if (chained_ && !prev_name.empty()) {
        entry_ordering.after.push_back(prev_name);
      }

      prev_name = sys->name();

      if (shared_gate) {
        sys = std::make_unique<ConditionedSystem>(std::move(sys), shared_gate);
      }

      result.push_back({std::move(sys), std::move(entry_ordering)});
    }

    return result;
  }
};

template <typename T> struct is_system_group_descriptor : std::false_type {};
template <>
struct is_system_group_descriptor<SystemGroupDescriptor> : std::true_type {};
template <typename T>
inline constexpr bool is_system_group_descriptor_v =
    is_system_group_descriptor<T>::value;

template <typename F> SystemDescriptor<decay_t<F>> group(F &&func) {
  return SystemDescriptor<decay_t<F>>(std::forward<F>(func));
}

template <typename F1, typename F2, typename... Fs>
SystemGroupDescriptor group(F1 &&f1, F2 &&f2, Fs &&...fs) {
  return SystemGroupDescriptor(std::forward<F1>(f1), std::forward<F2>(f2),
                               std::forward<Fs>(fs)...);
}

struct SetConfig {
  SetId id;
  std::vector<SetId> after;
  std::vector<SetId> before;
  std::shared_ptr<ICondition> condition;
};

class SetConfigDescriptor {
  SetConfig config_;

public:
  explicit SetConfigDescriptor(SetId id)
      : config_{.id = id, .after = {}, .before = {}, .condition = nullptr} {}

  [[nodiscard]] const SetId &id() const { return config_.id; }

  SetConfigDescriptor &&before(SetId id) && {
    config_.before.push_back(id);
    return std::move(*this);
  }

  SetConfigDescriptor &&after(SetId id) && {
    config_.after.push_back(id);
    return std::move(*this);
  }

  template <typename Cond> SetConfigDescriptor &&run_if(Cond &&cond) && {
    config_.condition = make_condition(std::forward<Cond>(cond));
    return std::move(*this);
  }

  SetConfigDescriptor &&run_if(std::unique_ptr<ICondition> cond) && {
    config_.condition = std::move(cond);
    return std::move(*this);
  }

  SetConfig take() { return std::move(config_); }
};

class ChainedSetConfigDescriptor {
  std::vector<SetId> ids_;
  bool chained_ = false;

public:
  explicit ChainedSetConfigDescriptor(std::vector<SetId> ids)
      : ids_(std::move(ids)) {}

  [[nodiscard]] const std::vector<SetId> &ids() const { return ids_; }

  ChainedSetConfigDescriptor &&chain() && {
    chained_ = true;
    return std::move(*this);
  }

  std::vector<SetConfig> take() {
    if (!chained_) {
      throw std::runtime_error("configure(...) with multiple sets requires "
                               ".chain()");
    }

    detail::ensure_unique_sets(ids_, "configure(...).chain()");

    std::vector<SetConfig> configs;
    configs.reserve(ids_.size());
    for (usize i = 0; i < ids_.size(); ++i) {
      SetConfig config{
          .id = ids_[i], .after = {}, .before = {}, .condition = nullptr};
      if (i + 1 < ids_.size()) {
        config.before.push_back(ids_[i + 1]);
      }
      configs.push_back(std::move(config));
    }
    return configs;
  }
};

template <typename E>
  requires std::is_enum_v<E>
SetConfigDescriptor configure(E value) {
  return SetConfigDescriptor(set(value));
}

template <typename S>
  requires std::is_same_v<typename S::T, Set> || std::is_base_of_v<Set, S>
SetConfigDescriptor configure() {
  return SetConfigDescriptor(set<S>());
}

template <typename A, typename B, typename... Rest>
  requires(detail::is_set_argument_v<A> && detail::is_set_argument_v<B> &&
           (... && detail::is_set_argument_v<Rest>))
ChainedSetConfigDescriptor configure(A &&a, B &&b, Rest &&...rest) {
  return ChainedSetConfigDescriptor(detail::make_set_ids(
      std::forward<A>(a), std::forward<B>(b), std::forward<Rest>(rest)...));
}

template <typename A, typename B, typename... Rest>
  requires(detail::is_set_argument_v<A> && detail::is_set_argument_v<B> &&
           (... && detail::is_set_argument_v<Rest>))
SetList sets(A &&a, B &&b, Rest &&...rest) {
  auto ids = detail::make_set_ids(std::forward<A>(a), std::forward<B>(b),
                                  std::forward<Rest>(rest)...);
  detail::ensure_unique_sets(ids, "sets(...)");
  return SetList{std::move(ids)};
}

} // namespace ecs
