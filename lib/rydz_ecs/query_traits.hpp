#pragma once
#include "access.hpp"
#include "entity.hpp"
#include "storage.hpp"
#include "world.hpp"
#include <cstddef>
#include <span>

namespace ecs {

template <typename T, typename StoragePtr = storage_t<T> const*>
struct FetcherBase {
  StoragePtr storage = nullptr;

  [[nodiscard]] auto size() const -> usize {
    return storage ? storage->size() : 0;
  }
  [[nodiscard]] auto is_required() const -> bool { return true; }
  [[nodiscard]] auto entities() const -> std::span<Entity const> {
    return storage ? storage->entities() : std::span<Entity const>{};
  }
};

template <typename T> struct WorldQueryTraits {
  using Item = T const*;

  static auto access(SystemAccess& acc) -> void {
    acc.add_component_read<T>();
    acc.add_archetype_required<T>();
  }
  static auto is_valid(Item item) -> bool { return item != nullptr; }

  struct Fetcher : FetcherBase<T> {
    auto init(World& world) -> void { this->storage = world.get_storage<T>(); }

    auto fetch(Entity entity) const -> Item {
      return this->storage ? this->storage->get(entity) : nullptr;
    }
  };
};

template <typename T> struct WorldQueryTraits<Mut<T>> {
  using Item = Mut<T>;

  static auto access(SystemAccess& acc) -> void {
    acc.add_component_write<T>();
    acc.add_archetype_required<T>();
  }
  static auto is_valid(Item const& item) -> bool { return item.ptr != nullptr; }

  struct Fetcher : FetcherBase<T, storage_t<T>*> {
    Tick tick;

    auto init(World& world) -> void {
      this->storage = world.get_storage<T>();
      tick = world.read_change_tick();
    }
    auto fetch(Entity entity) const -> Item {
      if (!this->storage) {
        return Item{};
      }
      auto [p, t] = this->storage->get_with_ticks(entity);
      return Item{p, t, tick};
    }
  };
};

template <typename T> struct WorldQueryTraits<Opt<T>> : WorldQueryTraits<T> {
  static auto access(SystemAccess& acc) -> void { acc.add_component_read<T>(); }
  static auto is_valid(typename WorldQueryTraits<T>::Item) -> bool {
    return true;
  }

  struct Fetcher : WorldQueryTraits<T>::Fetcher {
    [[nodiscard]] auto size() const -> usize { return SIZE_MAX; }
    [[nodiscard]] auto is_required() const -> bool { return false; }
  };
};

template <typename T>
struct WorldQueryTraits<Opt<Mut<T>>> : WorldQueryTraits<Mut<T>> {
  static auto access(SystemAccess& acc) -> void {
    acc.add_component_write<T>();
  }
  static auto is_valid(typename WorldQueryTraits<Mut<T>>::Item const&) -> bool {
    return true;
  }

  struct Fetcher : WorldQueryTraits<Mut<T>>::Fetcher {
    [[nodiscard]] auto size() const -> usize { return SIZE_MAX; }
    [[nodiscard]] auto is_required() const -> bool { return false; }
  };
};

template <> struct WorldQueryTraits<Entity> {
  using Item = Entity;

  static auto access(SystemAccess&) -> void {}
  static auto is_valid(Item) -> bool { return true; }

  struct Fetcher {
    auto init(World&) -> void {}
    static auto fetch(Entity entity) -> Item { return entity; }
    static auto size() -> usize { return SIZE_MAX; }
    static auto is_required() -> bool { return false; }
    static auto entities() -> std::span<Entity const> { return {}; }
  };
};

} // namespace ecs
