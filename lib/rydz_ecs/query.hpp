#pragma once
#include "filter.hpp"
#include "helpers.hpp"
#include "params.hpp"
#include "query_traits.hpp"
#include "query_types.hpp"
#include <cstdio>
#include <optional>
#include <print>
#include <tuple>
#include <utility>

namespace ecs {

template <typename... Qs> class Query {
  using Decomposed = detail::SplitTrailingFilters<Qs...>;
  using ItemTuple = typename Decomposed::Items;
  using FilterT = typename Decomposed::FilterList;

  template <typename T> class SingleQueryResult {
    std::optional<T> value_;

  public:
    SingleQueryResult() = default;
    SingleQueryResult(std::nullopt_t) : value_(std::nullopt) {}
    SingleQueryResult(T value) : value_(std::move(value)) {}
    SingleQueryResult(std::optional<T> value) : value_(std::move(value)) {}

    explicit operator bool() const { return value_.has_value(); }

    auto operator*() -> T& { return *value_; }
    auto operator*() const -> T const& { return *value_; }

    auto operator->() -> T { return *value_; }
    auto operator->() const -> T { return *value_; }
  };

  template <typename... Items> struct ResultTypeFor;
  template <typename... Items> struct ResultTypeFor<Tuple<Items...>> {
    using type = std::conditional_t<
      sizeof...(Items) == 1,
      SingleQueryResult<std::tuple_element_t<0, Tuple<Items...>>>,
      std::optional<Tuple<Items...>>>;
  };

  template <typename... Items> struct PreparedState {
    Tuple<typename WorldQueryTraits<Items>::Fetcher...> fetchers;
    std::span<Entity const> candidates;
  };

  template <typename TupleT> struct PreparedStateFor;
  template <typename... Items> struct PreparedStateFor<Tuple<Items...>> {
    using type = PreparedState<Items...>;
  };

  using CachedState = typename PreparedStateFor<ItemTuple>::type;

  World* world_;
  Tick last_run_;
  Tick this_run_;
  mutable std::optional<CachedState> cached_state_;

public:
  explicit Query(World& world, Tick last_run, Tick this_run)
      : world_(&world), last_run_(last_run), this_run_(this_run) {}

  explicit Query(World& world) : Query(world, Tick{}, Tick{}) {}

  template <typename Func> void each(Func&& func) const {
    for_each_impl(std::forward<Func>(func), ItemTuple{});
  }

  auto iter() const { return make_iter(ItemTuple{}); }

  auto empty() const -> bool { return is_empty(); }

  auto is_empty() const -> bool { return is_empty_impl(ItemTuple{}); }

  auto get(Entity entity) const { return get_impl(ItemTuple{}, entity); }

  auto single() const { return single_impl(ItemTuple{}); }

  static auto access(SystemAccess& acc) -> void {
    access_items(acc, ItemTuple{});
    QueryFilterTraits<FilterT>::access(acc);
  }

private:
  template <typename Q> auto make_fetcher() const {
    typename WorldQueryTraits<Q>::Fetcher fetcher;
    fetcher.init(*world_);
    return fetcher;
  }

  auto prepared_query() const -> CachedState const& {
    if (!cached_state_.has_value()) {
      cached_state_.emplace(prepare_query(ItemTuple{}));
    }

    return *cached_state_;
  }

  template <typename... Items>
  auto find_smallest_entities_group(
    Tuple<typename WorldQueryTraits<Items>::Fetcher...> const& fetchers
  ) const -> std::span<Entity const> {
    usize min_size = SIZE_MAX;
    std::span<Entity const> result;
    std::apply(
      [&](auto const&... f) -> auto {
        auto check = [&](auto const& fetcher) -> auto {
          if (fetcher.is_required() && fetcher.size() < min_size) {
            min_size = fetcher.size();
            result = fetcher.entities();
          }
        };
        (check(f), ...);
      },
      fetchers
    );

    if (min_size == SIZE_MAX) {
      usize filter_size = QueryFilterTraits<FilterT>::candidate_size(*world_);
      if (filter_size < min_size) {
        result = QueryFilterTraits<FilterT>::candidates(*world_);
      }
    }

    return result;
  }

  template <typename... Items>
  auto prepare_query(Tuple<Items...>) const -> PreparedState<Items...> {
    auto fetchers = std::make_tuple(make_fetcher<Items>()...);
    auto candidates = find_smallest_entities_group<Items...>(fetchers);
    return {
      .fetchers = std::move(fetchers),
      .candidates = candidates,
    };
  }

  template <typename... Items, typename Fetchers>
  static auto fetch_all(Fetchers const& fetchers, Entity entity) {
    return std::apply(
      [&](auto const&... f) -> auto { return Tuple{f.fetch(entity)...}; }, fetchers
    );
  }

  template <typename... Items>
  static auto all_valid(Tuple<typename WorldQueryTraits<Items>::Item...> const& items)
    -> bool {
    return [&]<size_t... I>(std::index_sequence<I...>) -> auto {
      return (WorldQueryTraits<Items>::is_valid(std::get<I>(items)) && ...);
    }(std::index_sequence_for<Items...>{});
  }

  template <typename... Items>
  static auto try_fetch(
    auto const& fetchers, Entity entity, World const& world, Tick last_run, Tick this_run
  ) -> std::optional<Tuple<typename WorldQueryTraits<Items>::Item...>> {
    auto items = fetch_all<Items...>(fetchers, entity);
    if (!all_valid<Items...>(items) ||
        !QueryFilterTraits<FilterT>::matches(world, entity, last_run, this_run)) {
      return std::nullopt;
    }
    return items;
  }

  template <typename... Items> auto single_impl(Tuple<Items...>) const {
    using Result =
      typename ResultTypeFor<Tuple<typename WorldQueryTraits<Items>::Item...>>::type;
    auto const& prepared = prepared_query();

    Result result{};
    for (Entity entity : prepared.candidates) {
      auto fetched =
        try_fetch<Items...>(prepared.fetchers, entity, *world_, last_run_, this_run_);
      if (!fetched) {
        continue;
      }
      if (result) {
        std::println(stderr, "Query::single() found more than one match");
        return Result{std::nullopt};
      }
      result = flatten_result(*fetched);
    }

    if (!result) {
      std::println(stderr, "Query::single() found no matches\n");
    }

    return result;
  }

  template <typename... Items> auto is_empty_impl(Tuple<Items...>) const -> bool {
    auto const& prepared = prepared_query();

    for (Entity entity : prepared.candidates) {
      if (try_fetch<Items...>(prepared.fetchers, entity, *world_, last_run_, this_run_)) {
        return false;
      }
    }

    return true;
  }

  template <typename... Items> auto get_impl(Tuple<Items...>, Entity entity) const {
    using Result =
      typename ResultTypeFor<Tuple<typename WorldQueryTraits<Items>::Item...>>::type;
    auto const& prepared = prepared_query();
    auto fetched =
      try_fetch<Items...>(prepared.fetchers, entity, *world_, last_run_, this_run_);
    if (!fetched) {
      return Result{std::nullopt};
    }
    return Result{flatten_result(*fetched)};
  }

  template <typename... Items> auto make_iter(Tuple<Items...>) const {
    auto const& prepared = prepared_query();
    auto fetchers = prepared.fetchers;
    auto candidates = prepared.candidates;

    return candidates |
           std::views::transform(
             [fetchers, world = world_, lr = last_run_, tr = this_run_](
               Entity entity
             ) -> auto { return try_fetch<Items...>(fetchers, entity, *world, lr, tr); }
           ) |
           std::views::filter([](auto const& opt) -> auto { return opt.has_value(); }) |
           std::views::transform([](auto&& opt) -> auto {
             return *std::forward<decltype(opt)>(opt);
           });
  }

  template <typename Func, typename... Items>
  auto for_each_impl(Func&& func, Tuple<Items...>) const -> void {
    auto const& prepared = prepared_query();

    for (Entity entity : prepared.candidates) {
      auto fetched =
        try_fetch<Items...>(prepared.fetchers, entity, *world_, last_run_, this_run_);
      if (!fetched) {
        continue;
      }
      std::apply(func, *fetched);
    }
  }

  template <typename... Items>
  static auto access_items(SystemAccess& acc, Tuple<Items...>) -> void {
    (WorldQueryTraits<Items>::access(acc), ...);
  }

  template <typename... Items> static auto flatten_result(Tuple<Items...> items) {
    if constexpr (sizeof...(Items) == 1) {
      return std::get<0>(std::move(items));
    } else {
      return items;
    }
  }
};

template <typename... Qs>
struct SystemParamTraits<Query<Qs...>> : DefaultSystemParamState<Query<Qs...>> {
  using Item = Query<Qs...>;

  static auto access(SystemAccess& acc) -> void { Query<Qs...>::access(acc); }

  static auto retrieve(World& world, SystemContext const& ctx) -> Item {
    return Query<Qs...>(world, ctx.last_run, ctx.this_run);
  }
  static auto available(World const&) -> bool { return true; }
};

} // namespace ecs
