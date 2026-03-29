#pragma once
#include "access.hpp"
#include "query.hpp"
#include "world.hpp"
#include <array>
#include <concepts>
#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace ecs {

struct NonSendMarker {};

template <typename T> struct Res {
  const T *ptr;
  const T &operator*() const { return *ptr; }
  const T *operator->() const { return ptr; }
};

template <typename T> struct ResMut {
  T *ptr;
  T &operator*() { return *ptr; }
  T *operator->() { return ptr; }
  const T &operator*() const { return *ptr; }
  const T *operator->() const { return ptr; }
};

class ISystem {
public:
  virtual ~ISystem() = default;
  virtual void run(World &world) = 0;
  virtual std::string name() const = 0;
  virtual std::string type_name() const { return name(); }
  virtual SystemAccess access() const { return {}; }
};

template <typename P> struct SystemParamTraits {};

template <typename T> struct SystemParamTraits<Res<T>> {
  using Item = Res<T>;

  static Item retrieve(World &world) {
    const T *ptr = world.get_resource<T>();
    if (!ptr) {
      throw std::runtime_error(std::string("Resource not found: ") +
                               typeid(T).name());
    }
    return Res<T>{ptr};
  }

  static bool available(const World &world) { return world.has_resource<T>(); }
  static void access(SystemAccess &acc) { acc.add_resource_read<T>(); }
};

template <typename T> struct SystemParamTraits<ResMut<T>> {
  using Item = ResMut<T>;

  static Item retrieve(World &world) {
    T *ptr = world.get_resource<T>();
    if (!ptr) {
      throw std::runtime_error(std::string("Resource not found: ") +
                               typeid(T).name());
    }
    return ResMut<T>{ptr};
  }

  static bool available(const World &world) { return world.has_resource<T>(); }
  static void access(SystemAccess &acc) { acc.add_resource_write<T>(); }
};

template <typename... Qs> struct SystemParamTraits<Query<Qs...>> {
  using Item = Query<Qs...>;

  static void access(SystemAccess &acc) { Query<Qs...>::access(acc); }

  static Item retrieve(World &world) { return Query<Qs...>(world); }
  static bool available(const World &) { return true; }
};

template <> struct SystemParamTraits<World> {
  using Item = World &;

  static void access(SystemAccess &acc) {
    acc.set_exclusive();
    acc.set_main_thread_only();
  }

  static Item retrieve(World &world) { return world; }
  static bool available(const World &) { return true; }
};

template <> struct SystemParamTraits<NonSendMarker> {
  using Item = NonSendMarker;

  static void access(SystemAccess &acc) { acc.set_main_thread_only(); }

  static Item retrieve(World &) { return {}; }
  static bool available(const World &) { return true; }
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
concept SystemParameter = requires(World &w, SystemAccess &acc) {
  SystemParamTraits<bare_t<T>>::retrieve(w);
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
    function_traits<F>::apply(
        [&]<SystemParameter... Args>() { run_with_args<Args...>(world); });
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
  template <SystemParameter... Args> void run_with_args(World &world) {
    func_(SystemParamTraits<bare_t<Args>>::retrieve(world)...);
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
