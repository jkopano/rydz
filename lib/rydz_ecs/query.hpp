#pragma once
#include "access.hpp"
#include "entity.hpp"
#include "filter.hpp"
#include "helpers.hpp"
#include "rl.hpp"
#include "storage.hpp"
#include "ticks.hpp"
#include "types.hpp"
#include "world.hpp"
#include <optional>
#include <span>
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

template <typename T, typename StoragePtr = const storage_t<T> *>
struct FetcherBase {
  StoragePtr storage = nullptr;

  usize size() const { return storage ? storage->size() : 0; }
  bool is_required() const { return true; }

  std::span<const Entity> entities() const {
    return storage ? storage->entities() : std::span<const Entity>{};
  }
};

template <typename T> struct WorldQueryTraits {
  using Item = const T *;

  static void access(SystemAccess &acc) { acc.add_component_read<T>(); }
  static bool is_valid(Item item) { return item != nullptr; }

  struct Fetcher : FetcherBase<T> {
    void init(World &world) { this->storage = world.get_storage<T>(); }

    Item fetch(Entity entity) const {
      return this->storage ? this->storage->get(entity) : nullptr;
    }
  };
};

template <typename T> struct WorldQueryTraits<Mut<T>> {
  using Item = Mut<T>;

  static void access(SystemAccess &acc) { acc.add_component_write<T>(); }
  static bool is_valid(const Item &item) { return item.ptr != nullptr; }

  struct Fetcher : FetcherBase<T, storage_t<T> *> {
    Tick tick{};

    void init(World &world) {
      this->storage = world.get_storage<T>();
      tick = world.read_change_tick();
    }
    Item fetch(Entity entity) const {
      return Item{this->storage ? this->storage->get(entity) : nullptr,
                  this->storage, entity, tick};
    }
  };
};

template <typename T> struct WorldQueryTraits<Opt<T>> {
  using Item = const T *;

  static void access(SystemAccess &acc) { acc.add_component_read<T>(); }
  static bool is_valid(Item) { return true; }

  struct Fetcher : FetcherBase<T> {
    void init(World &world) { this->storage = world.get_storage<T>(); }

    Item fetch(Entity entity) const {
      return this->storage ? this->storage->get(entity) : nullptr;
    }
    usize size() const { return SIZE_MAX; }
    bool is_required() const { return false; }
  };
};

template <typename T> struct WorldQueryTraits<Opt<Mut<T>>> {
  using Item = Mut<T>;

  static void access(SystemAccess &acc) { acc.add_component_write<T>(); }
  static bool is_valid(const Item &) { return true; }

  struct Fetcher : FetcherBase<T, storage_t<T> *> {
    Tick tick{};

    void init(World &world) {
      this->storage = world.get_storage<T>();
      tick = world.read_change_tick();
    }
    Item fetch(Entity entity) const {
      return Item{this->storage ? this->storage->get(entity) : nullptr,
                  this->storage, entity, tick};
    }
    usize size() const { return SIZE_MAX; }
    bool is_required() const { return false; }
  };
};

template <> struct WorldQueryTraits<Entity> {
  using Item = Entity;

  static void access(SystemAccess &) {}
  static bool is_valid(Item) { return true; }

  struct Fetcher {
    void init(World &) {}
    Item fetch(Entity entity) const { return entity; }
    usize size() const { return SIZE_MAX; }
    bool is_required() const { return false; }
    std::span<const Entity> entities() const { return {}; }
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
  template <typename Q> auto make_fetcher() const {
    typename WorldQueryTraits<Q>::Fetcher f;
    f.init(*world_);
    return f;
  }

  template <typename... Items>
  std::span<const Entity> find_smallest_entities_group(
      const std::tuple<typename WorldQueryTraits<Items>::Fetcher...> &fetchers)
      const {
    usize min_size = SIZE_MAX;
    std::span<const Entity> result;
    std::apply(
        [&](const auto &...f) {
          auto check = [&](const auto &fetcher) {
            if (fetcher.is_required() && fetcher.size() < min_size) {
              min_size = fetcher.size();
              result = fetcher.entities();
            }
          };
          (check(f), ...);
        },
        fetchers);

    if (min_size == SIZE_MAX) {
      usize filter_size = QueryFilterTraits<FilterT>::candidate_size(*world_);
      if (filter_size < min_size) {
        result = QueryFilterTraits<FilterT>::candidates(*world_);
      }
    }

    return result;
  }

  template <typename... Items, typename Fetchers>
  static auto fetch_all(const Fetchers &fetchers, Entity entity) {
    return std::apply(
        [&](const auto &...f) { return std::tuple{f.fetch(entity)...}; },
        fetchers);
  }

  template <typename... Items>
  static bool all_valid(
      const std::tuple<typename WorldQueryTraits<Items>::Item...> &items) {
    return [&]<size_t... I>(std::index_sequence<I...>) {
      return (WorldQueryTraits<Items>::is_valid(std::get<I>(items)) && ...);
    }(std::index_sequence_for<Items...>{});
  }

  template <typename... Items>
  static std::optional<std::tuple<typename WorldQueryTraits<Items>::Item...>>
  try_fetch(const auto &fetchers, Entity entity, const World &world) {
    auto items = fetch_all<Items...>(fetchers, entity);
    if (!all_valid<Items...>(items) ||
        !QueryFilterTraits<FilterT>::matches(world, entity))
      return std::nullopt;
    return items;
  }

  template <typename... Items> auto single_impl(std::tuple<Items...>) const {
    using Result = std::tuple<typename WorldQueryTraits<Items>::Item...>;
    auto fetchers = std::make_tuple(make_fetcher<Items>()...);
    auto candidates = find_smallest_entities_group<Items...>(fetchers);

    std::optional<Result> result;
    for (Entity entity : candidates) {
      auto fetched = try_fetch<Items...>(fetchers, entity, *world_);
      if (!fetched)
        continue;
      if (result.has_value()) {
        rl::TraceLog(LOG_INFO, "Query::single() found more than one match");
        return std::optional<Result>{std::nullopt};
      }
      result = *fetched;
    }

    if (!result.has_value())
      rl::TraceLog(LOG_DEBUG, "Query::single() found no matches");

    return result;
  }

  template <typename... Items> auto make_iter(std::tuple<Items...>) const {
    auto fetchers = std::make_tuple(make_fetcher<Items>()...);
    auto candidates = find_smallest_entities_group<Items...>(fetchers);

    return candidates |
           views::transform([fetchers, world = world_](Entity entity) {
             return try_fetch<Items...>(fetchers, entity, *world);
           }) |
           views::filter([](const auto &opt) { return opt.has_value(); }) |
           views::transform([](auto &&opt) { return *std::move(opt); });
  }

  template <typename Func, typename... Items>
  void for_each_impl(Func &&func, std::tuple<Items...>) const {
    auto fetchers = std::make_tuple(make_fetcher<Items>()...);
    auto candidates = find_smallest_entities_group<Items...>(fetchers);

    for (Entity entity : candidates) {
      auto fetched = try_fetch<Items...>(fetchers, entity, *world_);
      if (!fetched)
        continue;
      std::apply(func, *fetched);
    }
  }

  template <typename... Items>
  static void access_items(SystemAccess &acc, std::tuple<Items...>) {
    (WorldQueryTraits<Items>::access(acc), ...);
  }
};

} // namespace ecs
