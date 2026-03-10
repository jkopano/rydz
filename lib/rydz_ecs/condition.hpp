#pragma once
#include "system.hpp"
#include <memory>
#include <string>
#include <vector>

namespace ecs {

struct SystemOrdering {
  std::vector<std::string> after;
  std::vector<std::string> before;
};

template <typename Fn> std::string system_name_of(Fn &&fn) {
  using D = std::decay_t<Fn>;
  if constexpr (std::is_pointer_v<D> &&
                std::is_function_v<std::remove_pointer_t<D>>) {
    return std::to_string(reinterpret_cast<uintptr_t>(fn));
  } else {
    return typeid(D).name();
  }
}

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
    return evaluate(world, (typename function_traits<F>::args_tuple *)nullptr);
  }

private:
  template <typename... Args>
  bool evaluate(World &world, std::tuple<Args...> *) {
    return func_(SystemParamTraits<bare_param_t<Args>>::retrieve(world)...);
  }
};

class ConditionedSystem : public ISystem {
  std::unique_ptr<ISystem> system_;
  std::unique_ptr<ICondition> condition_;

public:
  ConditionedSystem(std::unique_ptr<ISystem> system,
                    std::unique_ptr<ICondition> condition)
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
  return std::make_unique<FunctionCondition<std::decay_t<F>>>(
      std::forward<F>(func));
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

template <typename F>
std::unique_ptr<ISystem> make_system_run_if(F &&func,
                                            std::unique_ptr<ICondition> cond) {
  return std::make_unique<ConditionedSystem>(make_system(std::forward<F>(func)),
                                             std::move(cond));
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

template <typename F> SystemDescriptor<std::decay_t<F>> into_system(F &&func) {
  return SystemDescriptor<std::decay_t<F>>(std::forward<F>(func));
}

} // namespace ecs
