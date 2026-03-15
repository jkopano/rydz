#pragma once
#include "access.hpp"
#include "entity.hpp"
#include "filter.hpp"
#include "helpers.hpp"
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
  SparseSetStorage<T> *storage = nullptr;
  Entity entity{};
  Tick tick{};
  bool marked = false;

  Mut() = default;
  Mut(T *p, SparseSetStorage<T> *s, Entity e, Tick t)
      : ptr(p), storage(s), entity(e), tick(t) {}

  T &get() {
    mark();
    return *ptr;
  }
  const T &get() const { return *ptr; }

  T *operator->() {
    mark();
    return ptr;
  }
  const T *operator->() const { return ptr; }

  T &operator*() {
    mark();
    return *ptr;
  }
  const T &operator*() const { return *ptr; }

  explicit operator bool() const { return ptr != nullptr; }

  operator T *() {
    mark();
    return ptr;
  }
  operator const T *() const { return ptr; }

  T &bypass_change_detection() { return *ptr; }

private:
  void mark() {
    if (!marked && storage && ptr) {
      storage->mark_changed(entity, tick);
      marked = true;
    }
  }
};
template <typename Inner> struct Opt {};

template <typename T> struct WorldQueryTraits {
  using Item = const T *;

  static void access(SystemAccess &acc) { acc.add_component_read<T>(); }

  struct Fetcher {
    const SparseSetStorage<T> *storage = nullptr;

    void init(World &world) { storage = world.get_storage<T>(); }

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

template <typename T> struct WorldQueryTraits<Mut<T>> {
  using Item = Mut<T>;

  static void access(SystemAccess &acc) { acc.add_component_write<T>(); }

  struct Fetcher {
    SparseSetStorage<T> *storage = nullptr;
    Tick tick{};

    void init(World &world) {
      storage = world.get_storage<T>();
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

template <typename T> struct WorldQueryTraits<Opt<T>> {
  using Item = const T *;

  static void access(SystemAccess &acc) { acc.add_component_read<T>(); }

  struct Fetcher {
    const SparseSetStorage<T> *storage = nullptr;

    void init(World &world) { storage = world.get_storage<T>(); }
    Item fetch(Entity entity) const {
      return storage ? storage->get(entity) : nullptr;
    }
    bool matches(Entity) const { return true; }
    size_t capacity() const { return SIZE_MAX; }
    bool is_required() const { return false; }
  };
};

template <typename T> struct WorldQueryTraits<Opt<Mut<T>>> {
  using Item = Mut<T>;

  static void access(SystemAccess &acc) { acc.add_component_write<T>(); }

  struct Fetcher {
    SparseSetStorage<T> *storage = nullptr;
    Tick tick{};

    void init(World &world) {
      storage = world.get_storage<T>();
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

template <> struct WorldQueryTraits<Entity> {
  using Item = Entity;

  static void access(SystemAccess &) {}

  struct Fetcher {
    void init(World &) {}
    Item fetch(Entity entity) const { return entity; }
    bool matches(Entity) const { return true; }
    size_t capacity() const { return SIZE_MAX; }
    bool is_required() const { return false; }
  };
};

template <typename... Qs> class Query {
  using Decomposed = detail::SplitTrailingFilters<Qs...>;
  using ItemTuple = typename Decomposed::Items;
  using FilterT = typename Decomposed::FilterList;

  World *world_;

public:
  explicit Query(World &world) : world_(&world) {}

  template <typename Func> void each(Func &&func) const {
    for_each_impl(std::forward<Func>(func), ItemTuple{});
  }

  auto iter() const { return make_iter(ItemTuple{}); }

  auto single() const { return single_impl(ItemTuple{}); }

  static void access(SystemAccess &acc) {
    access_items(acc, ItemTuple{});
    QueryFilterTraits<FilterT>::access(acc);
  }

private:
  template <typename... Items> auto single_impl(std::tuple<Items...>) const {
    using ResultTuple = std::tuple<typename WorldQueryTraits<Items>::Item...>;
    auto fetchers = std::make_tuple(make_fetcher<Items>()...);
    size_t min_cap = compute_min_cap<Items...>(fetchers);

    std::optional<ResultTuple> result;
    for (auto i : range(0u, min_cap)) {
      Entity entity = Entity::from_raw(i, 0);
      if (!matches_all<Items...>(fetchers, entity))
        continue;
      if (!QueryFilterTraits<FilterT>::matches(*world_, entity))
        continue;

      if (result.has_value()) {
        rl::TraceLog(LOG_INFO, "Query::single() found more than one match");
        return std::optional<ResultTuple>{std::nullopt};
      }
      result = std::apply(
          [&](const auto &...f) { return std::tuple{f.fetch(entity)...}; },
          fetchers);
    }

    if (!result.has_value())
      rl::TraceLog(LOG_INFO, "Query::single() found no matches");

    return result;
  }

  template <typename Q> auto make_fetcher() const {
    typename WorldQueryTraits<Q>::Fetcher f;
    f.init(*world_);
    return f;
  }

  template <typename... Items>
  static size_t
  compute_min_cap(const std::tuple<typename WorldQueryTraits<Items>::Fetcher...>
                      &fetchers) {
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
    return (min_cap == SIZE_MAX) ? 0 : min_cap;
  }

  template <typename... Items>
  static bool matches_all(
      const std::tuple<typename WorldQueryTraits<Items>::Fetcher...> &fetchers,
      Entity entity) {
    return std::apply(
        [&](const auto &...f) { return (f.matches(entity) && ...); }, fetchers);
  }

  template <typename... Items> auto make_iter(std::tuple<Items...>) const {
    auto fetchers = std::make_tuple(make_fetcher<Items>()...);
    uint32_t count =
        static_cast<uint32_t>(compute_min_cap<Items...>(fetchers));

    auto matches = [fetchers, world = world_](uint32_t index) {
      Entity entity = Entity::from_raw(index, 0);
      if (!matches_all<Items...>(fetchers, entity))
        return false;
      return QueryFilterTraits<FilterT>::matches(*world, entity);
    };

    auto fetch = [fetchers](uint32_t index) {
      Entity entity = Entity::from_raw(index, 0);
      return std::apply(
          [&](const auto &...f) { return std::tuple{f.fetch(entity)...}; },
          fetchers);
    };

    return views::iota(uint32_t{0}, count) | views::filter(matches) |
           views::transform(fetch);
  }

  template <typename Func, typename... Items>
  void for_each_impl(Func &&func, std::tuple<Items...>) const {
    auto fetchers = std::make_tuple(make_fetcher<Items>()...);
    size_t min_cap = compute_min_cap<Items...>(fetchers);

    for (auto i : range(0u, min_cap)) {
      Entity entity = Entity::from_raw(i, 0);

      if (!matches_all<Items...>(fetchers, entity))
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
