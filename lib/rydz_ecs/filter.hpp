#pragma once
#include "access.hpp"
#include "ticks.hpp"
#include "world.hpp"
#include <span>
#include <tuple>
#include <type_traits>

namespace ecs {

template <typename T> struct With {};
template <typename T> struct Without {};
template <typename T> struct Added {};
template <typename T> struct Changed {};
template <typename F1, typename F2> struct Or {};
template <typename... Fs> struct Filters {};

template <typename F> struct QueryFilterTraits;

template <> struct QueryFilterTraits<Filters<>> {
  static bool matches(const World &, Entity) { return true; }
  static void access(SystemAccess &) {}
  static std::span<const Entity> candidates(const World &) { return {}; }
  static usize candidate_size(const World &) { return SIZE_MAX; }
};

template <typename F> struct QueryFilterTraits<Filters<F>> {
  static bool matches(const World &world, Entity entity) {
    return QueryFilterTraits<F>::matches(world, entity);
  }
  static void access(SystemAccess &acc) { QueryFilterTraits<F>::access(acc); }
  static std::span<const Entity> candidates(const World &world) {
    return QueryFilterTraits<F>::candidates(world);
  }
  static usize candidate_size(const World &world) {
    return QueryFilterTraits<F>::candidate_size(world);
  }
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
  static std::span<const Entity> candidates(const World &world) {
    usize size_f = QueryFilterTraits<F>::candidate_size(world);
    usize size_rest = QueryFilterTraits<Filters<Fs...>>::candidate_size(world);
    if (size_f <= size_rest)
      return QueryFilterTraits<F>::candidates(world);
    return QueryFilterTraits<Filters<Fs...>>::candidates(world);
  }
  static usize candidate_size(const World &world) {
    usize size_f = QueryFilterTraits<F>::candidate_size(world);
    usize size_rest = QueryFilterTraits<Filters<Fs...>>::candidate_size(world);
    return size_f < size_rest ? size_f : size_rest;
  }
};

template <typename T> struct QueryFilterTraits<With<T>> {
  static bool matches(const World &world, Entity entity) {
    return world.has_component<T>(entity);
  }
  static void access(SystemAccess &acc) { acc.add_component_read<T>(); }

  static std::span<const Entity> candidates(const World &world) {
    auto *storage = world.get_storage<T>();
    return storage ? storage->entities() : std::span<const Entity>{};
  }
  static usize candidate_size(const World &world) {
    auto *storage = world.get_storage<T>();
    return storage ? storage->size() : 0;
  }
};

template <typename T> struct QueryFilterTraits<Without<T>> {
  static bool matches(const World &world, Entity entity) {
    return !world.has_component<T>(entity);
  }

  static void access(SystemAccess &) {}
  static std::span<const Entity> candidates(const World &) { return {}; }
  static usize candidate_size(const World &) { return SIZE_MAX; }
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

  static std::span<const Entity> candidates(const World &world) {
    auto *storage = world.get_storage<T>();
    return storage ? storage->entities() : std::span<const Entity>{};
  }

  static usize candidate_size(const World &world) {
    auto *storage = world.get_storage<T>();
    return storage ? storage->size() : 0;
  }
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

  static std::span<const Entity> candidates(const World &world) {
    auto *storage = world.get_storage<T>();
    return storage ? storage->entities() : std::span<const Entity>{};
  }

  static usize candidate_size(const World &world) {
    auto *storage = world.get_storage<T>();
    return storage ? storage->size() : 0;
  }
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

  static std::span<const Entity> candidates(const World &) { return {}; }

  static usize candidate_size(const World &) { return SIZE_MAX; }
};

template <typename T> struct is_filter : std::false_type {};
template <typename T> struct is_filter<With<T>> : std::true_type {};
template <typename T> struct is_filter<Without<T>> : std::true_type {};
template <typename T> struct is_filter<Added<T>> : std::true_type {};
template <typename T> struct is_filter<Changed<T>> : std::true_type {};
template <typename F1, typename F2>
struct is_filter<Or<F1, F2>> : std::true_type {};
template <typename... Fs> struct is_filter<Filters<Fs...>> : std::true_type {};

namespace detail {

template <typename F, typename FiltersT> struct MergeFilters;

template <typename F, typename... Fs> struct MergeFilters<F, Filters<Fs...>> {
  using type = Filters<F, Fs...>;
};

template <typename... Fs1, typename... Fs2>
struct MergeFilters<Filters<Fs1...>, Filters<Fs2...>> {
  using type = Filters<Fs1..., Fs2...>;
};

template <typename... Qs> struct SplitTrailingFilters;

template <> struct SplitTrailingFilters<> {
  using Items = std::tuple<>;
  using FilterList = Filters<>;
};

template <typename T, typename... Rest>
struct SplitTrailingFilters<T, Rest...> {
  using Tail = SplitTrailingFilters<Rest...>;
  static constexpr bool tail_has_items =
      !std::is_same_v<typename Tail::Items, std::tuple<>>;
  static constexpr bool is_filter_v = is_filter<T>::value;

  static_assert(!(tail_has_items && is_filter_v),
                "Query filters must be trailing parameters");

  using Items =
      std::conditional_t<is_filter_v && !tail_has_items, typename Tail::Items,
                         decltype(std::tuple_cat(
                             std::declval<std::tuple<T>>(),
                             std::declval<typename Tail::Items>()))>;

  using FilterList = std::conditional_t<
      is_filter_v && !tail_has_items,
      typename MergeFilters<T, typename Tail::FilterList>::type,
      typename Tail::FilterList>;
};

} // namespace detail

} // namespace ecs
