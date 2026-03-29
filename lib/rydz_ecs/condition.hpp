#pragma once
#include "fwd.hpp"
#include "system.hpp"
#include <memory>
#include <string>
#include <type_traits>
#include <typeindex>
#include <vector>

namespace ecs {

struct SetId {
  std::type_index type;
  i32 value;

  SetId(std::type_index t, i32 v) : type(t), value(v) {}

  bool operator==(const SetId &o) const {
    return type == o.type && value == o.value;
  }
};

template <typename E>
  requires std::is_enum_v<E>
SetId set(E value) {
  return SetId{typeid(E), static_cast<i32>(value)};
}

template <typename S>
  requires std::is_same_v<typename S::T, Set> ||
           std::is_base_of<Set, S>::value
SetId set() {
  return SetId{typeid(S), 0};
}

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

struct SystemOrdering {
  std::vector<std::string> after;
  std::vector<std::string> before;
  std::vector<SetId> in_sets;
};

class ICondition {
public:
  virtual ~ICondition() = default;
  virtual bool is_true(World &world) = 0;
};

template <typename F> class FunctionCondition : public ICondition {
  F func_;

public:
  explicit FunctionCondition(F func) : func_(std::move(func)) {}

  bool is_true(World &world) override {
    return function_traits<F>::apply([&]<SystemParameter... Args>() {
      return func_(SystemParamTraits<bare_t<Args>>::retrieve(world)...);
    });
  }
};

class ConditionedSystem : public ISystem {
  std::unique_ptr<ISystem> system_;
  std::shared_ptr<ICondition> condition_;

public:
  ConditionedSystem(std::unique_ptr<ISystem> system,
                    std::shared_ptr<ICondition> condition)
      : system_(std::move(system)), condition_(std::move(condition)) {}

  void run(World &world) override {
    if (condition_->is_true(world)) {
      system_->run(world);
    }
  }

  std::string name() const override { return system_->name(); }

  SystemAccess access() const override { return system_->access(); }
};

template <typename F> std::unique_ptr<ICondition> make_condition(F &&func) {
  return std::make_unique<FunctionCondition<decay_t<F>>>(std::forward<F>(func));
}

inline std::unique_ptr<ICondition> run_once() {
  struct RunOnceCondition : ICondition {
    bool fired = false;
    bool is_true(World & /*world*/) override {
      if (!fired) {
        fired = true;
        return true;
      }
      return false;
    }
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

  SystemDescriptor &&in_set(SetId id) && {
    ordering_.in_sets.push_back(id);
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

  SystemGroupDescriptor &&in_set(SetId id) && {
    ordering_.in_sets.push_back(id);
    return std::move(*this);
  }

  SystemGroupDescriptor &&chain() && {
    chained_ = true;
    return std::move(*this);
  }

  std::vector<GroupSystemEntry> build() {
    std::vector<GroupSystemEntry> result;

    std::shared_ptr<ICondition> shared_cond = std::move(condition_);

    std::string prev_name;
    for (auto &sys : systems_) {
      SystemOrdering entry_ordering = ordering_;

      if (chained_ && !prev_name.empty())
        entry_ordering.after.push_back(prev_name);

      prev_name = sys->name();

      if (shared_cond)
        sys = std::make_unique<ConditionedSystem>(std::move(sys), shared_cond);

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
  explicit SetConfigDescriptor(SetId id) : config_{id, {}, {}, nullptr} {}

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

template <typename E>
  requires std::is_enum_v<E>
SetConfigDescriptor configure(E value) {
  return SetConfigDescriptor(set(value));
}

template <typename S>
  requires std::is_same_v<typename S::T, Set> ||
           std::is_base_of<Set, S>::value
SetConfigDescriptor configure() {
  return SetConfigDescriptor(set<S>());
}

} // namespace ecs
