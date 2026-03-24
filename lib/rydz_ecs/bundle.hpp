#pragma once
#include "fwd.hpp"
#include "world.hpp"
#include <tuple>
#include <type_traits>

namespace ecs {
template <typename T, typename = void> struct type_tag_of {
  using type = Component;
};

template <typename T> struct type_tag_of<T, std::void_t<typename T::Type>> {
  using type = typename T::Type;
};

template <typename T>
concept ComponentTrait = std::is_same_v<typename type_tag_of<T>::type, Component>;

template <typename T>
concept ResourceTrait = requires { typename T::Type; } &&
                        std::is_same_v<typename type_tag_of<T>::type, Resource>;

template <typename T>
concept BundleTrait = requires { typename T::Type; } &&
                      std::is_same_v<typename type_tag_of<T>::type, Bundle>;

template <typename T>
concept EventTrait =
    requires { typename T::Type; } && std::is_same_v<typename T::Type, Event>;

template <typename T> using StripAll = std::remove_cvref_t<T>;

template <typename T>
concept Spawnable = ComponentTrait<StripAll<T>> || BundleTrait<StripAll<T>>;

template <typename R>
concept SpawnableRange =
    std::ranges::input_range<R> && Spawnable<std::ranges::range_value_t<R>>;

namespace detail {

struct any_field {
  template <typename T> constexpr operator T() const;
};

template <typename T, typename... Args>
concept aggregate_init = requires { T{std::declval<Args>()...}; };

// clang-format off
template <typename T> consteval int field_count() {
  if constexpr (!std::is_aggregate_v<T>) return 0;
  else if constexpr (aggregate_init<T, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field>) return 16;
  else if constexpr (aggregate_init<T, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field>) return 15;
  else if constexpr (aggregate_init<T, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field>) return 14;
  else if constexpr (aggregate_init<T, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field>) return 13;
  else if constexpr (aggregate_init<T, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field>) return 12;
  else if constexpr (aggregate_init<T, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field>) return 11;
  else if constexpr (aggregate_init<T, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field>) return 10;
  else if constexpr (aggregate_init<T, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field>) return 9;
  else if constexpr (aggregate_init<T, any_field, any_field, any_field, any_field, any_field, any_field, any_field, any_field>) return 8;
  else if constexpr (aggregate_init<T, any_field, any_field, any_field, any_field, any_field, any_field, any_field>) return 7;
  else if constexpr (aggregate_init<T, any_field, any_field, any_field, any_field, any_field, any_field>) return 6;
  else if constexpr (aggregate_init<T, any_field, any_field, any_field, any_field, any_field>) return 5;
  else if constexpr (aggregate_init<T, any_field, any_field, any_field, any_field>) return 4;
  else if constexpr (aggregate_init<T, any_field, any_field, any_field>) return 3;
  else if constexpr (aggregate_init<T, any_field, any_field>) return 2;
  else if constexpr (aggregate_init<T, any_field>) return 1;
  else return 0;
}

template <typename T> auto to_tuple(T &&t) {
  using Raw = StripAll<T>;
  constexpr int N = field_count<Raw>();
  if constexpr (N == 0) { return std::tuple{}; }
  else if constexpr (N == 1) { auto&& [a] = std::forward<T>(t); return std::make_tuple(std::forward<decltype(a)>(a)); }
  else if constexpr (N == 2) { auto&& [a,b] = std::forward<T>(t); return std::make_tuple(std::forward<decltype(a)>(a), std::forward<decltype(b)>(b)); }
  else if constexpr (N == 3) { auto&& [a,b,c] = std::forward<T>(t); return std::make_tuple(std::forward<decltype(a)>(a), std::forward<decltype(b)>(b), std::forward<decltype(c)>(c)); }
  else if constexpr (N == 4) { auto&& [a,b,c,d] = std::forward<T>(t); return std::make_tuple(std::forward<decltype(a)>(a), std::forward<decltype(b)>(b), std::forward<decltype(c)>(c), std::forward<decltype(d)>(d)); }
  else if constexpr (N == 5) { auto&& [a,b,c,d,e] = std::forward<T>(t); return std::make_tuple(std::forward<decltype(a)>(a), std::forward<decltype(b)>(b), std::forward<decltype(c)>(c), std::forward<decltype(d)>(d), std::forward<decltype(e)>(e)); }
  else if constexpr (N == 6) { auto&& [a,b,c,d,e,f] = std::forward<T>(t); return std::make_tuple(std::forward<decltype(a)>(a), std::forward<decltype(b)>(b), std::forward<decltype(c)>(c), std::forward<decltype(d)>(d), std::forward<decltype(e)>(e), std::forward<decltype(f)>(f)); }
  else if constexpr (N == 7) { auto&& [a,b,c,d,e,f,g] = std::forward<T>(t); return std::make_tuple(std::forward<decltype(a)>(a), std::forward<decltype(b)>(b), std::forward<decltype(c)>(c), std::forward<decltype(d)>(d), std::forward<decltype(e)>(e), std::forward<decltype(f)>(f), std::forward<decltype(g)>(g)); }
  else if constexpr (N == 8) { auto&& [a,b,c,d,e,f,g,h] = std::forward<T>(t); return std::make_tuple(std::forward<decltype(a)>(a), std::forward<decltype(b)>(b), std::forward<decltype(c)>(c), std::forward<decltype(d)>(d), std::forward<decltype(e)>(e), std::forward<decltype(f)>(f), std::forward<decltype(g)>(g), std::forward<decltype(h)>(h)); }
  else if constexpr (N == 9) { auto&& [a,b,c,d,e,f,g,h,i] = std::forward<T>(t); return std::make_tuple(std::forward<decltype(a)>(a), std::forward<decltype(b)>(b), std::forward<decltype(c)>(c), std::forward<decltype(d)>(d), std::forward<decltype(e)>(e), std::forward<decltype(f)>(f), std::forward<decltype(g)>(g), std::forward<decltype(h)>(h), std::forward<decltype(i)>(i)); }
  else if constexpr (N == 10) { auto&& [a,b,c,d,e,f,g,h,i,j] = std::forward<T>(t); return std::make_tuple(std::forward<decltype(a)>(a), std::forward<decltype(b)>(b), std::forward<decltype(c)>(c), std::forward<decltype(d)>(d), std::forward<decltype(e)>(e), std::forward<decltype(f)>(f), std::forward<decltype(g)>(g), std::forward<decltype(h)>(h), std::forward<decltype(i)>(i), std::forward<decltype(j)>(j)); }
  else if constexpr (N == 11) { auto&& [a,b,c,d,e,f,g,h,i,j,k] = std::forward<T>(t); return std::make_tuple(std::forward<decltype(a)>(a), std::forward<decltype(b)>(b), std::forward<decltype(c)>(c), std::forward<decltype(d)>(d), std::forward<decltype(e)>(e), std::forward<decltype(f)>(f), std::forward<decltype(g)>(g), std::forward<decltype(h)>(h), std::forward<decltype(i)>(i), std::forward<decltype(j)>(j), std::forward<decltype(k)>(k)); }
  else if constexpr (N == 12) { auto&& [a,b,c,d,e,f,g,h,i,j,k,l] = std::forward<T>(t); return std::make_tuple(std::forward<decltype(a)>(a), std::forward<decltype(b)>(b), std::forward<decltype(c)>(c), std::forward<decltype(d)>(d), std::forward<decltype(e)>(e), std::forward<decltype(f)>(f), std::forward<decltype(g)>(g), std::forward<decltype(h)>(h), std::forward<decltype(i)>(i), std::forward<decltype(j)>(j), std::forward<decltype(k)>(k), std::forward<decltype(l)>(l)); }
  else if constexpr (N == 13) { auto&& [a,b,c,d,e,f,g,h,i,j,k,l,m] = std::forward<T>(t); return std::make_tuple(std::forward<decltype(a)>(a), std::forward<decltype(b)>(b), std::forward<decltype(c)>(c), std::forward<decltype(d)>(d), std::forward<decltype(e)>(e), std::forward<decltype(f)>(f), std::forward<decltype(g)>(g), std::forward<decltype(h)>(h), std::forward<decltype(i)>(i), std::forward<decltype(j)>(j), std::forward<decltype(k)>(k), std::forward<decltype(l)>(l), std::forward<decltype(m)>(m)); }
  else if constexpr (N == 14) { auto&& [a,b,c,d,e,f,g,h,i,j,k,l,m,n] = std::forward<T>(t); return std::make_tuple(std::forward<decltype(a)>(a), std::forward<decltype(b)>(b), std::forward<decltype(c)>(c), std::forward<decltype(d)>(d), std::forward<decltype(e)>(e), std::forward<decltype(f)>(f), std::forward<decltype(g)>(g), std::forward<decltype(h)>(h), std::forward<decltype(i)>(i), std::forward<decltype(j)>(j), std::forward<decltype(k)>(k), std::forward<decltype(l)>(l), std::forward<decltype(m)>(m), std::forward<decltype(n)>(n)); }
  else if constexpr (N == 15) { auto&& [a,b,c,d,e,f,g,h,i,j,k,l,m,n,o] = std::forward<T>(t); return std::make_tuple(std::forward<decltype(a)>(a), std::forward<decltype(b)>(b), std::forward<decltype(c)>(c), std::forward<decltype(d)>(d), std::forward<decltype(e)>(e), std::forward<decltype(f)>(f), std::forward<decltype(g)>(g), std::forward<decltype(h)>(h), std::forward<decltype(i)>(i), std::forward<decltype(j)>(j), std::forward<decltype(k)>(k), std::forward<decltype(l)>(l), std::forward<decltype(m)>(m), std::forward<decltype(n)>(n), std::forward<decltype(o)>(o)); }
  else if constexpr (N == 16) { auto&& [a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p] = std::forward<T>(t); return std::make_tuple(std::forward<decltype(a)>(a), std::forward<decltype(b)>(b), std::forward<decltype(c)>(c), std::forward<decltype(d)>(d), std::forward<decltype(e)>(e), std::forward<decltype(f)>(f), std::forward<decltype(g)>(g), std::forward<decltype(h)>(h), std::forward<decltype(i)>(i), std::forward<decltype(j)>(j), std::forward<decltype(k)>(k), std::forward<decltype(l)>(l), std::forward<decltype(m)>(m), std::forward<decltype(n)>(n), std::forward<decltype(o)>(o), std::forward<decltype(p)>(p)); }
}
// clang-format on

template <typename T> struct is_tuple : std::false_type {};
template <typename... Ts>
struct is_tuple<std::tuple<Ts...>> : std::true_type {};
template <typename T> inline constexpr bool is_tuple_v = is_tuple<T>::value;

template <typename T> void insert_single(World &world, Entity entity, T &&item);

template <typename... Ts>
void insert_tuple_items(World &world, Entity entity, std::tuple<Ts...> &&tup) {
  std::apply(
      [&](auto &&...elems) {
        (insert_single(world, entity, std::forward<decltype(elems)>(elems)),
         ...);
      },
      std::move(tup));
}

template <typename T>
void insert_single(World &world, Entity entity, T &&item) {
  using Raw = StripAll<T>;
  if constexpr (BundleTrait<Raw>) {
    insert_tuple_items(world, entity, to_tuple(std::forward<T>(item)));
  } else if constexpr (is_tuple_v<Raw>) {
    insert_tuple_items(world, entity, std::forward<T>(item));
  } else {
    static_assert(ComponentTrait<Raw>,
                  "Only Components and Bundles can be inserted on entities. "
                  "Add 'using Type = Component;' or 'using Type = Bundle;'.");
    world.insert_component(entity, std::forward<T>(item));
  }
}

} // namespace detail

template <typename T>
void insert_bundle(World &world, Entity entity, T &&item) {
  detail::insert_single(world, entity, std::forward<T>(item));
}

template <typename... Ts>
void insert_bundle(World &world, Entity entity, Ts &&...items) {
  (detail::insert_single(world, entity, std::forward<Ts>(items)), ...);
}

template <std::ranges::input_range R>
  requires Spawnable<std::ranges::range_value_t<R>>
std::vector<Entity> spawn_batch(World &world, R &&_range) {
  std::vector<Entity> spawned;
  if constexpr (std::ranges::sized_range<R>)
    spawned.reserve(std::ranges::size(_range));

  for (auto &&item : std::forward<R>(range)) {
    Entity e = world.spawn();
    insert_bundle(world, e, std::forward<decltype(item)>(item));
    spawned.push_back(e);
  }
  return spawned;
}

} // namespace ecs
