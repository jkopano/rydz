#pragma once
#include "access.hpp"
#include "entity.hpp"
#include "storage.hpp"
#include "world.hpp"
#include <cstddef>
#include <span>

namespace ecs {

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

  static void access(SystemAccess &acc) {
    acc.add_component_read<T>();
    acc.add_archetype_required<T>();
  }
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

  static void access(SystemAccess &acc) {
    acc.add_component_write<T>();
    acc.add_archetype_required<T>();
  }
  static bool is_valid(const Item &item) { return item.ptr != nullptr; }

  struct Fetcher : FetcherBase<T, storage_t<T> *> {
    Tick tick{};

    void init(World &world) {
      this->storage = world.get_storage<T>();
      tick = world.read_change_tick();
    }
    Item fetch(Entity entity) const {
      if (!this->storage)
        return Item{};
      auto [p, t] = this->storage->get_with_ticks(entity);
      return Item{p, t, tick};
    }
  };
};

template <typename T> struct WorldQueryTraits<Opt<T>> : WorldQueryTraits<T> {
  static void access(SystemAccess &acc) { acc.add_component_read<T>(); }
  static bool is_valid(typename WorldQueryTraits<T>::Item) { return true; }

  struct Fetcher : WorldQueryTraits<T>::Fetcher {
    usize size() const { return SIZE_MAX; }
    bool is_required() const { return false; }
  };
};

template <typename T>
struct WorldQueryTraits<Opt<Mut<T>>> : WorldQueryTraits<Mut<T>> {
  static void access(SystemAccess &acc) { acc.add_component_write<T>(); }
  static bool is_valid(const typename WorldQueryTraits<Mut<T>>::Item &) {
    return true;
  }

  struct Fetcher : WorldQueryTraits<Mut<T>>::Fetcher {
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

} // namespace ecs
