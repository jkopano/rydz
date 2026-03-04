#pragma once
#include "access.hpp"
#include "query.hpp"
#include "world.hpp"
#include <functional>
#include <memory>
#include <string>
#include <type_traits>

namespace ecs {

// Marker parameter to force a system to run on the main thread.
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
  virtual SystemAccess access() const { return {}; }
};

template <typename P> struct SystemParamTraits;

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

  static bool available(const World &world) {
    return world.contains_resource<T>();
  }

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

  static bool available(const World &world) {
    return world.contains_resource<T>();
  }

  static void access(SystemAccess &acc) { acc.add_resource_write<T>(); }
};

template <typename... Qs> struct SystemParamTraits<Query<Qs...>> {
  using Item = Query<Qs...>;

  static Item retrieve(World &world) { return Query<Qs...>(world); }

  static bool available(const World &) { return true; }

  static void access(SystemAccess &acc) { Query<Qs...>::access(acc); }
};

template <> struct SystemParamTraits<World> {
  using Item = World &;

  static Item retrieve(World &world) { return world; }

  static bool available(const World &) { return true; }

  static void access(SystemAccess &acc) { acc.set_exclusive(); }
};

template <> struct SystemParamTraits<NonSendMarker> {
  using Item = NonSendMarker;

  static Item retrieve(World & /*world*/) { return {}; }

  static bool available(const World & /*world*/) { return true; }

  static void access(SystemAccess &acc) { acc.set_main_thread_only(); }
};

namespace detail {

template <typename R, typename... Args> struct function_traits_impl {
  using return_type = R;
  using args_tuple = std::tuple<Args...>;
  static constexpr size_t arity = sizeof...(Args);

  template <size_t N> using arg = std::tuple_element_t<N, args_tuple>;
};

} // namespace detail

template <typename F>
struct function_traits : function_traits<decltype(&F::operator())> {};

template <typename R, typename... Args>
struct function_traits<R (*)(Args...)>
    : detail::function_traits_impl<R, Args...> {};

template <typename C, typename R, typename... Args>
struct function_traits<R (C::*)(Args...)>
    : detail::function_traits_impl<R, Args...> {};

template <typename C, typename R, typename... Args>
struct function_traits<R (C::*)(Args...) const>
    : detail::function_traits_impl<R, Args...> {};

template <typename R, typename... Args>
struct function_traits<R (*)(Args...) noexcept>
    : detail::function_traits_impl<R, Args...> {};

template <typename C, typename R, typename... Args>
struct function_traits<R (C::*)(Args...) noexcept>
    : detail::function_traits_impl<R, Args...> {};

template <typename C, typename R, typename... Args>
struct function_traits<R (C::*)(Args...) const noexcept>
    : detail::function_traits_impl<R, Args...> {};

template <typename F> struct function_traits<F &> : function_traits<F> {};

template <typename F> struct function_traits<F &&> : function_traits<F> {};

template <typename T>
using bare_param_t = std::remove_cv_t<std::remove_reference_t<T>>;

template <typename F> class FunctionSystem : public ISystem {
  F func_;
  std::string name_;

public:
  explicit FunctionSystem(F func, std::string name = "")
      : func_(std::move(func)),
        name_(name.empty() ? typeid(F).name() : std::move(name)) {}

  void run(World &world) override {
    run_impl(world, (typename function_traits<F>::args_tuple *)nullptr);
  }

  std::string name() const override { return name_; }

  SystemAccess access() const override {
    SystemAccess acc;
    access_impl(acc, (typename function_traits<F>::args_tuple *)nullptr);
    return acc;
  }

private:
  template <typename... Args>
  void run_impl(World &world, std::tuple<Args...> *) {
    func_(SystemParamTraits<bare_param_t<Args>>::retrieve(world)...);
  }

  template <typename... Args>
  static void access_impl(SystemAccess &acc, std::tuple<Args...> *) {
    (SystemParamTraits<bare_param_t<Args>>::access(acc), ...);
  }
};

template <typename F>
std::unique_ptr<ISystem> make_system(F &&func, std::string name = "") {
  return std::make_unique<FunctionSystem<std::decay_t<F>>>(
      std::forward<F>(func), std::move(name));
}

template <typename Sched, typename F>
void add_single_system(Sched &schedule, F &&func) {
  schedule.add_system(make_system(std::forward<F>(func)));
}

} // namespace ecs
