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
  static auto matches(World const&, Entity, Tick, Tick) -> bool { return true; }
  static auto access(SystemAccess&) -> void {}
  static auto candidates(World const&) -> std::span<Entity const> { return {}; }
  static auto candidate_size(World const&) -> usize { return SIZE_MAX; }
};

template <typename F> struct QueryFilterTraits<Filters<F>> {
  static auto matches(
    World const& world, Entity entity, Tick last_run, Tick this_run
  ) -> bool {
    return QueryFilterTraits<F>::matches(world, entity, last_run, this_run);
  }
  static auto access(SystemAccess& acc) -> void {
    QueryFilterTraits<F>::access(acc);
  }
  static auto candidates(World const& world) -> std::span<Entity const> {
    return QueryFilterTraits<F>::candidates(world);
  }
  static auto candidate_size(World const& world) -> usize {
    return QueryFilterTraits<F>::candidate_size(world);
  }
};

template <typename F, typename... Fs>
struct QueryFilterTraits<Filters<F, Fs...>> {
  static auto matches(
    World const& world, Entity entity, Tick last_run, Tick this_run
  ) -> bool {
    return QueryFilterTraits<F>::matches(world, entity, last_run, this_run) &&
           QueryFilterTraits<Filters<Fs...>>::matches(
             world, entity, last_run, this_run
           );
  }
  static auto access(SystemAccess& acc) -> void {
    QueryFilterTraits<F>::access(acc);
    QueryFilterTraits<Filters<Fs...>>::access(acc);
  }
  static auto candidates(World const& world) -> std::span<Entity const> {
    usize size_f = QueryFilterTraits<F>::candidate_size(world);
    usize size_rest = QueryFilterTraits<Filters<Fs...>>::candidate_size(world);
    if (size_f <= size_rest) {
      return QueryFilterTraits<F>::candidates(world);
    }
    return QueryFilterTraits<Filters<Fs...>>::candidates(world);
  }
  static auto candidate_size(World const& world) -> usize {
    usize size_f = QueryFilterTraits<F>::candidate_size(world);
    usize size_rest = QueryFilterTraits<Filters<Fs...>>::candidate_size(world);
    return size_f < size_rest ? size_f : size_rest;
  }
};

template <typename T> struct QueryFilterTraits<With<T>> {
  static auto matches(World const& world, Entity entity, Tick, Tick) -> bool {
    return world.has_component<T>(entity);
  }
  static auto access(SystemAccess& acc) -> void {
    acc.add_component_read<T>();
    acc.add_archetype_required<T>();
  }

  static auto candidates(World const& world) -> std::span<Entity const> {
    auto* storage = world.get_storage<T>();
    return storage ? storage->entities() : std::span<Entity const>{};
  }
  static auto candidate_size(World const& world) -> usize {
    auto* storage = world.get_storage<T>();
    return storage ? storage->size() : 0;
  }
};

template <typename T> struct QueryFilterTraits<Without<T>> {
  static auto matches(World const& world, Entity entity, Tick, Tick) -> bool {
    return !world.has_component<T>(entity);
  }

  static auto access(SystemAccess& acc) -> void {
    acc.add_archetype_excluded<T>();
  }
  static auto candidates(World const&) -> std::span<Entity const> { return {}; }
  static auto candidate_size(World const&) -> usize { return SIZE_MAX; }
};

template <typename T> struct QueryFilterTraits<Added<T>> {
  static auto matches(
    World const& world, Entity entity, Tick last_run, Tick this_run
  ) -> bool {
    auto ticks = world.get_component_ticks<T>(entity);
    if (!ticks) {
      return false;
    }

    return ticks->added.is_newer_than(last_run, this_run);
  }
  static auto access(SystemAccess& acc) -> void {
    acc.add_component_read<T>();
    acc.add_archetype_required<T>();
  }

  static auto candidates(World const& world) -> std::span<Entity const> {
    auto* storage = world.get_storage<T>();
    return storage ? storage->entities() : std::span<Entity const>{};
  }

  static auto candidate_size(World const& world) -> usize {
    auto* storage = world.get_storage<T>();
    return storage ? storage->size() : 0;
  }
};

template <typename T> struct QueryFilterTraits<Changed<T>> {
  static auto matches(
    World const& world, Entity entity, Tick last_run, Tick this_run
  ) -> bool {
    auto ticks = world.get_component_ticks<T>(entity);
    if (!ticks) {
      return false;
    }
    return ticks->changed.is_newer_than(last_run, this_run);
  }

  static auto access(SystemAccess& acc) -> void {
    acc.add_component_read<T>();
    acc.add_archetype_required<T>();
  }

  static auto candidates(World const& world) -> std::span<Entity const> {
    auto* storage = world.get_storage<T>();
    return storage ? storage->entities() : std::span<Entity const>{};
  }

  static auto candidate_size(World const& world) -> usize {
    auto* storage = world.get_storage<T>();
    return storage ? storage->size() : 0;
  }
};

template <typename F1, typename F2> struct QueryFilterTraits<Or<F1, F2>> {
  static auto matches(
    World const& world, Entity entity, Tick last_run, Tick this_run
  ) -> bool {
    return QueryFilterTraits<F1>::matches(world, entity, last_run, this_run) ||
           QueryFilterTraits<F2>::matches(world, entity, last_run, this_run);
  }

  static auto access(SystemAccess& acc) -> void {
    SystemAccess temp_acc;
    QueryFilterTraits<F1>::access(temp_acc);
    QueryFilterTraits<F2>::access(temp_acc);

    acc.components_read.insert(
      temp_acc.components_read.begin(), temp_acc.components_read.end()
    );
    acc.components_write.insert(
      temp_acc.components_write.begin(), temp_acc.components_write.end()
    );
    acc.resources_read.insert(
      temp_acc.resources_read.begin(), temp_acc.resources_read.end()
    );
    acc.resources_write.insert(
      temp_acc.resources_write.begin(), temp_acc.resources_write.end()
    );
  }

  static auto candidates(World const&) -> std::span<Entity const> { return {}; }

  static auto candidate_size(World const&) -> usize { return SIZE_MAX; }
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
  using Items = Tuple<>;
  using FilterList = Filters<>;
};

template <typename T, typename... Rest>
struct SplitTrailingFilters<T, Rest...> {
  using Tail = SplitTrailingFilters<Rest...>;
  static constexpr bool tail_has_items =
    !std::is_same_v<typename Tail::Items, Tuple<>>;
  static constexpr bool is_filter_v = is_filter<T>::value;

  static_assert(
    !(tail_has_items && is_filter_v),
    "Query filters must be trailing parameters"
  );

  using Items = std::conditional_t<
    is_filter_v && !tail_has_items,
    typename Tail::Items,
    decltype(std::tuple_cat(
      std::declval<Tuple<T>>(), std::declval<typename Tail::Items>()
    ))>;

  using FilterList = std::conditional_t<
    is_filter_v && !tail_has_items,
    typename MergeFilters<T, typename Tail::FilterList>::type,
    typename Tail::FilterList>;
};

} // namespace detail

} // namespace ecs
