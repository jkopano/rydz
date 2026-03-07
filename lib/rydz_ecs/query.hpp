#pragma once
#include "access.hpp"
#include "entity.hpp"
#include "storage.hpp"
#include "ticks.hpp"
#include "world.hpp"
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace ecs {

template <typename T> struct Mut {
  T *ptr = nullptr;
  VecStorage<T> *storage = nullptr;
  Entity entity{};
  Tick tick{};
  bool dirty = false;

  Mut() = default;
  Mut(T *p, VecStorage<T> *s, Entity e, Tick t)
      : ptr(p), storage(s), entity(e), tick(t) {}

  T &get() {
    dirty = true;
    return *ptr;
  }
  const T &get() const { return *ptr; }

  T *operator->() {
    dirty = true;
    return ptr;
  }
  const T *operator->() const { return ptr; }

  T &operator*() {
    dirty = true;
    return *ptr;
  }
  const T &operator*() const { return *ptr; }

  explicit operator bool() const { return ptr != nullptr; }

  operator T *() {
    dirty = true;
    return ptr;
  }
  operator const T *() const { return ptr; }

  ~Mut() {
    if (dirty && storage && ptr)
      storage->mark_changed(entity, tick);
  }
};
template <typename Inner> struct Opt {};

template <typename T> struct With {};
template <typename T> struct Without {};
template <typename T> struct Added {};
template <typename T> struct Changed {};
template <typename F1, typename F2> struct Or {};
template <typename... Fs> struct Filters {};

// Primary template: bare T = read-only access (const T*)
template <typename T> struct WorldQueryTraits {
  using Item = const T *;

  static void access(SystemAccess &acc) { acc.add_component_read<T>(); }

  struct Fetcher {
    const VecStorage<T> *storage = nullptr;

    void init(const World &world) { storage = world.get_storage<T>(); }

    Item fetch(Entity entity) const {
      return storage ? storage->get(entity) : nullptr;
    }

    bool matches(Entity entity) const {
      return storage && storage->has(entity);
    }

    size_t capacity() const { return storage ? storage->data_size() : 0; }
    bool is_required() const { return true; }
  };
};

// Mut<T> = mutable access (T*)
template <typename T> struct WorldQueryTraits<Mut<T>> {
  using Item = Mut<T>;

  static void access(SystemAccess &acc) { acc.add_component_write<T>(); }

  struct Fetcher {
    VecStorage<T> *storage = nullptr;
    Tick tick{};

    void init(const World &world) {
      storage = const_cast<World &>(world).get_storage<T>();
      tick = world.read_change_tick();
    }
    Item fetch(Entity entity) const {
      return Item{storage ? storage->get(entity) : nullptr, storage, entity,
                  tick};
    }
    bool matches(Entity entity) const {
      return storage && storage->has(entity);
    }
    size_t capacity() const { return storage ? storage->data_size() : 0; }
    bool is_required() const { return true; }
  };
};

// Opt<T> = optional read (const T*, nullable)
template <typename T> struct WorldQueryTraits<Opt<T>> {
  using Item = const T *;

  static void access(SystemAccess &acc) { acc.add_component_read<T>(); }

  struct Fetcher {
    const VecStorage<T> *storage = nullptr;

    void init(const World &world) { storage = world.get_storage<T>(); }
    Item fetch(Entity entity) const {
      return storage ? storage->get(entity) : nullptr;
    }
    bool matches(Entity) const { return true; }
    size_t capacity() const { return SIZE_MAX; }
    bool is_required() const { return false; }
  };
};

// Opt<Mut<T>> = optional mutable (T*, nullable)
template <typename T> struct WorldQueryTraits<Opt<Mut<T>>> {
  using Item = Mut<T>;

  static void access(SystemAccess &acc) { acc.add_component_write<T>(); }

  struct Fetcher {
    VecStorage<T> *storage = nullptr;
    Tick tick{};

    void init(const World &world) {
      storage = const_cast<World &>(world).get_storage<T>();
      tick = world.read_change_tick();
    }
    Item fetch(Entity entity) const {
      return Item{storage ? storage->get(entity) : nullptr, storage, entity,
                  tick};
    }
    bool matches(Entity) const { return true; }
    size_t capacity() const { return SIZE_MAX; }
    bool is_required() const { return false; }
  };
};

template <typename F> struct QueryFilterTraits;

template <> struct QueryFilterTraits<Filters<>> {
  static bool matches(const World &, Entity) { return true; }
  static void access(SystemAccess &) {}
};

template <typename F> struct QueryFilterTraits<Filters<F>> {
  static bool matches(const World &world, Entity entity) {
    return QueryFilterTraits<F>::matches(world, entity);
  }
  static void access(SystemAccess &acc) { QueryFilterTraits<F>::access(acc); }
};

template <typename F, typename... Fs>
struct QueryFilterTraits<Filters<F, Fs...>> {
  static bool matches(const World &world, Entity entity) {
    return QueryFilterTraits<F>::matches(world, entity) &&
           QueryFilterTraits<Filters<Fs...>>::matches(world, entity);
  }
  static void access(SystemAccess &acc) {
    QueryFilterTraits<F>::access(acc);
    QueryFilterTraits<Filters<Fs...>>::access(acc);
  }
};

template <typename T> struct QueryFilterTraits<With<T>> {
  static bool matches(const World &world, Entity entity) {
    return world.has_component<T>(entity);
  }
  static void access(SystemAccess &acc) { acc.add_component_read<T>(); }
};

template <typename T> struct QueryFilterTraits<Without<T>> {
  static bool matches(const World &world, Entity entity) {
    return !world.has_component<T>(entity);
  }
  static void access(SystemAccess &) {}
};

template <typename T> struct QueryFilterTraits<Added<T>> {
  static bool matches(const World &world, Entity entity) {
    auto ticks = world.get_component_ticks<T>(entity);
    if (!ticks)
      return false;
    Tick this_run = world.read_change_tick();
    Tick last_run = Tick{this_run.value - 1};
    return ticks->added.is_newer_than(last_run, this_run);
  }
  static void access(SystemAccess &acc) { acc.add_component_read<T>(); }
};

template <typename T> struct QueryFilterTraits<Changed<T>> {
  static bool matches(const World &world, Entity entity) {
    auto ticks = world.get_component_ticks<T>(entity);
    if (!ticks)
      return false;
    Tick this_run = world.read_change_tick();
    Tick last_run = Tick{this_run.value - 1};
    return ticks->changed.is_newer_than(last_run, this_run);
  }
  static void access(SystemAccess &acc) { acc.add_component_read<T>(); }
};

template <typename F1, typename F2> struct QueryFilterTraits<Or<F1, F2>> {
  static bool matches(const World &world, Entity entity) {
    return QueryFilterTraits<F1>::matches(world, entity) ||
           QueryFilterTraits<F2>::matches(world, entity);
  }
  static void access(SystemAccess &acc) {
    QueryFilterTraits<F1>::access(acc);
    QueryFilterTraits<F2>::access(acc);
  }
};

template <typename T> struct is_filter : std::false_type {};
template <typename T> struct is_filter<With<T>> : std::true_type {};
template <typename T> struct is_filter<Without<T>> : std::true_type {};
template <typename T> struct is_filter<Added<T>> : std::true_type {};
template <typename T> struct is_filter<Changed<T>> : std::true_type {};
template <typename F1, typename F2> struct is_filter<Or<F1, F2>> : std::true_type {};
template <typename... Fs> struct is_filter<Filters<Fs...>> : std::true_type {};

namespace detail {

template <typename F, typename FiltersT> struct MergeFilters;

template <typename F, typename... Fs>
struct MergeFilters<F, Filters<Fs...>> {
  using type = Filters<F, Fs...>;
};

template <typename... Fs1, typename... Fs2>
struct MergeFilters<Filters<Fs1...>, Filters<Fs2...>> {
  using type = Filters<Fs1..., Fs2...>;
};

template <typename... Qs> struct SplitTrailingFilters;

template <> struct SplitTrailingFilters<> {
  using Items = std::tuple<>;
  using Filters = Filters<>;
};

template <typename T, typename... Rest>
struct SplitTrailingFilters<T, Rest...> {
  using Tail = SplitTrailingFilters<Rest...>;
  static constexpr bool tail_has_items =
      !std::is_same_v<typename Tail::Items, std::tuple<>>;
  static constexpr bool is_filter_v = is_filter<T>::value;

  static_assert(!(tail_has_items && is_filter_v),
                "Query filters must be trailing parameters");

  using Items = std::conditional_t<
      is_filter_v && !tail_has_items,
      typename Tail::Items,
      decltype(std::tuple_cat(std::declval<std::tuple<T>>(),
                              std::declval<typename Tail::Items>()))>;

  using Filters = std::conditional_t<
      is_filter_v && !tail_has_items,
      typename MergeFilters<T, typename Tail::Filters>::type,
      typename Tail::Filters>;
};

} // namespace detail

template <typename... Qs> class Query {
  using Decomposed = detail::SplitTrailingFilters<Qs...>;
  using ItemTuple = typename Decomposed::Items;
  using FilterT = typename Decomposed::Filters;

  const World *world_;

public:
  explicit Query(const World &world) : world_(&world) {}

  template <typename Func> void for_each(Func &&func) const {
    for_each_cached(std::forward<Func>(func), ItemTuple{});
  }

  static void access(SystemAccess &acc) {
    access_items(acc, ItemTuple{});
    QueryFilterTraits<FilterT>::access(acc);
  }

private:
  template <typename Q> auto make_fetcher() const {
    typename WorldQueryTraits<Q>::Fetcher f;
    f.init(*world_);
    return f;
  }

  template <typename Func, typename... Items>
  void for_each_cached(Func &&func, std::tuple<Items...>) const {

    auto fetchers = std::make_tuple(make_fetcher<Items>()...);

    size_t min_cap = SIZE_MAX;
    std::apply(
        [&](const auto &...f) {
          auto check = [&](const auto &fetcher) {
            if (fetcher.is_required() && fetcher.capacity() < min_cap)
              min_cap = fetcher.capacity();
          };
          (check(f), ...);
        },
        fetchers);

    if (min_cap == 0 || min_cap == SIZE_MAX)
      return;

    for (uint32_t i = 0; i < static_cast<uint32_t>(min_cap); i++) {
      Entity entity = Entity::from_raw(i, 0);

      bool all_match = std::apply(
          [&](const auto &...f) { return (f.matches(entity) && ...); },
          fetchers);
      if (!all_match)
        continue;

      if (!QueryFilterTraits<FilterT>::matches(*world_, entity))
        continue;

      std::apply([&](const auto &...f) { func(f.fetch(entity)...); }, fetchers);
    }
  }

  template <typename... Items>
  static void access_items(SystemAccess &acc, std::tuple<Items...>) {
    (WorldQueryTraits<Items>::access(acc), ...);
  }
};

} // namespace ecs
