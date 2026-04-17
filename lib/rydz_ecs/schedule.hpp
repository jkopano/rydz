#pragma once
#include "condition.hpp"
#include "schedule_label.hpp"
#include "system.hpp"
#include "tracy_plugin.hpp"
#include <algorithm>
#include <array>
#include <memory>
#include <sstream>
#include <string>
#include <taskflow/taskflow.hpp>
#include <unordered_map>
#include <variant>
#include <vector>

namespace ecs {

inline auto global_executor() -> tf::Executor & {
  static tf::Executor executor;
  return executor;
}

struct TaskPoolOptions {
  bool multithreaded = true;
};

struct ChainedSystems {
  struct Entry {
    std::unique_ptr<ISystem> system;
    SystemOrdering ordering;
  };
  std::vector<Entry> systems;
};

template <typename T> struct is_chained_systems : std::false_type {};
template <> struct is_chained_systems<ChainedSystems> : std::true_type {};
template <typename T>
inline constexpr bool is_chained_systems_v = is_chained_systems<T>::value;

namespace detail {

template <>
struct is_system_input_descriptor<ChainedSystems> : std::true_type {};

template <typename T>
concept ChainSystemInput =
    SystemCallable<T> || is_system_descriptor_v<bare_t<T>>;

template <typename Func>
  requires ChainSystemInput<Func>
auto make_chain_entry(Func &&func, const std::string &prev)
    -> ChainedSystems::Entry {
  SystemOrdering ordering;
  std::unique_ptr<ISystem> sys;

  if constexpr (is_system_descriptor_v<bare_t<Func>>) {
    ordering = func.take_ordering();
    sys = func.build();
  } else {
    sys = make_system(std::forward<Func>(func));
  }

  if (!prev.empty()) {
    ordering.after.push_back(prev);
  }

  return {.system = std::move(sys), .ordering = std::move(ordering)};
}

} // namespace detail

template <typename... Fs>
  requires(detail::ChainSystemInput<Fs> && ...)
auto chain(Fs &&...funcs) -> ChainedSystems {
  ChainedSystems result;
  std::string prev_name;

  auto process = [&](auto &&func) -> auto {
    auto entry =
        detail::make_chain_entry(std::forward<decltype(func)>(func), prev_name);
    prev_name = entry.system->name();
    result.systems.push_back(std::move(entry));
  };

  (process(std::forward<Fs>(funcs)), ...);
  return result;
}

class Schedule {
  struct SystemEntry {
    std::unique_ptr<ISystem> system;
    SystemAccess access;
    SystemOrdering ordering;
    std::vector<SetId> in_sets;
  };

  struct InlineStep {
    usize start, end;
  };

  struct ParallelBatch {
    usize start, end;
  };

  struct ParallelStep {
    std::vector<ParallelBatch> batches;
  };

  using ExecutionStep = std::variant<InlineStep, ParallelStep>;

  std::vector<SystemEntry> entries_;
  std::vector<ExecutionStep> steps_;
  std::vector<SetConfig> set_configs_;

  World *current_world_ = nullptr;
  bool needs_rebuild_ = true;

public:
  Schedule() = default;
  ~Schedule() = default;
  Schedule(Schedule &&) = default;
  auto operator=(Schedule &&) -> Schedule & = default;
  Schedule(const Schedule &) = delete;
  auto operator=(const Schedule &) -> Schedule & = delete;

  auto add_system(std::unique_ptr<ISystem> system, SystemOrdering ordering = {})
      -> void {
    SystemEntry entry;
    entry.system = std::move(system);
    entry.access = entry.system->access();
    entry.in_sets = std::move(ordering.in_sets);
    entry.ordering = std::move(ordering);
    entries_.push_back(std::move(entry));
    needs_rebuild_ = true;
  }

  auto add_set_config(SetConfig config) -> void {
    set_configs_.push_back(std::move(config));
    needs_rebuild_ = true;
  }

  template <typename F>
    requires SystemInput<F>
  auto add_system_fn(F &&func) -> void {
    add_system_fn_with_ordering(std::forward<F>(func), {});
  }

  template <typename F>
    requires(!SystemInput<F>)
  auto add_system_fn(F && /*func*/) -> void {
    detail::static_assert_valid_system_callable<F>();
  }

  template <typename E, typename F>
    requires(std::is_enum_v<bare_t<E>> &&
             !std::same_as<bare_t<E>, ScheduleLabel> && SystemInput<F>)
  auto add_system_fn(E set_id, F &&func) -> void {
    add_system_fn(set(set_id), std::forward<F>(func));
  }

  template <typename E, typename F>
    requires(std::is_enum_v<bare_t<E>> &&
             !std::same_as<bare_t<E>, ScheduleLabel> && !SystemInput<F>)
  auto add_system_fn(E /*set_id*/, F && /*func*/) -> void {
    detail::static_assert_valid_system_callable<F>();
  }

  template <typename S, typename F>
    requires(detail::is_set_marker_v<S> && SystemInput<F>)
  auto add_system_fn(S &&, F &&func) -> void {
    add_system_fn(set<bare_t<S>>(), std::forward<F>(func));
  }

  template <typename S, typename F>
    requires(detail::is_set_marker_v<S> && !SystemInput<F>)
  auto add_system_fn(S &&, F && /*func*/) -> void {
    detail::static_assert_valid_system_callable<F>();
  }

  template <typename S, typename F>
    requires((std::same_as<bare_t<S>, SetId> ||
              std::same_as<bare_t<S>, SetList>) &&
             SystemInput<F>)
  auto add_system_fn(S &&selector, F &&func) -> void {
    SystemOrdering ordering;
    ordering.in_sets = detail::to_set_ids(std::forward<S>(selector));
    detail::ensure_unique_sets(ordering.in_sets, "add_system_fn(...)");
    add_system_fn_with_ordering(std::forward<F>(func), std::move(ordering));
  }

  template <typename S, typename F>
    requires((std::same_as<bare_t<S>, SetId> ||
              std::same_as<bare_t<S>, SetList>) &&
             !SystemInput<F>)
  auto add_system_fn(S && /*selector*/, F && /*func*/) -> void {
    detail::static_assert_valid_system_callable<F>();
  }

  auto run(World &world) -> void {
    if (entries_.empty()) {
      return;
    }

    current_world_ = &world;
    prepare();
    world.begin_schedule_run();

    if (!world.multithreaded()) {
      for (auto &entry : entries_) {
        run_entry(static_cast<usize>(std::distance(entries_.data(), &entry)),
                  world);
      }
      return;
    }

    for (auto &step : steps_) {
      if (auto *s = std::get_if<InlineStep>(&step)) {
        for (usize i : range(s->start, s->end)) {
          run_entry(i, world);
        }
      } else {
        run_parallel_step(std::get<ParallelStep>(step), world);
      }
    }
  }

  [[nodiscard]] auto system_count() const -> usize { return entries_.size(); }
  [[nodiscard]] auto configured_set_count() const -> usize {
    return set_configs_.size();
  }
  [[nodiscard]] auto empty() const -> bool { return entries_.empty(); }

  auto prepare() -> void {
    if (needs_rebuild_) {
      rebuild();
      needs_rebuild_ = false;
    }
  }

#ifndef NDEBUG
  [[nodiscard]] auto debug_dump(const std::string &schedule_name) const
      -> std::string;
#endif

private:
  friend struct ScheduleDebug;

  template <typename F>
    requires SystemInput<F>
  auto add_system_fn_with_ordering(F &&func, SystemOrdering base_ordering)
      -> void {
    if constexpr (is_chained_systems_v<bare_t<F>>) {
      for (auto &e : func.systems) {
        merge_ordering(e.ordering, base_ordering);
        add_system(std::move(e.system), std::move(e.ordering));
      }

    } else if constexpr (is_system_group_descriptor_v<bare_t<F>>) {
      for (auto &entry : func.build()) {
        merge_ordering(entry.ordering, base_ordering);
        add_system(std::move(entry.system), std::move(entry.ordering));
      }

    } else if constexpr (is_system_descriptor_v<bare_t<F>>) {
      auto ordering = func.take_ordering();
      merge_ordering(ordering, base_ordering);
      add_system(func.build(), std::move(ordering));

    } else {
      add_system(make_system(std::forward<F>(func)), std::move(base_ordering));
    }
  }

  static auto merge_ordering(SystemOrdering &target,
                             const SystemOrdering &extra) -> void {
    target.after.insert(target.after.end(), extra.after.begin(),
                        extra.after.end());
    target.before.insert(target.before.end(), extra.before.begin(),
                         extra.before.end());
    target.in_sets.insert(target.in_sets.end(), extra.in_sets.begin(),
                          extra.in_sets.end());
  }

  auto rebuild() -> void {
    steps_.clear();

    const usize count = entries_.size();
    if (count == 0) {
      return;
    }

    apply_set_configs();
    topological_sort();
    build_execution_steps();
  }

  auto apply_set_configs() -> void {
    std::unordered_map<SetId, std::vector<usize>> set_members;
    for (usize i = 0; i < entries_.size(); ++i) {
      for (const auto &sid : entries_[i].in_sets) {
        set_members[sid].push_back(i);
      }
    }

    for (const auto &[sid, _] : set_members) {
      auto it = std::ranges::find_if(
          set_configs_, [&](auto &c) -> auto { return c.id == sid; });
      if (it == set_configs_.end()) {
        throw std::runtime_error(
            std::format("Set '{}' used but never configured", set_name(sid)));
      }
    }

    for (auto &cfg : set_configs_) {
      const auto &members = set_members[cfg.id];
      if (members.empty()) {
        continue;
      }

      // CONDITION
      if (cfg.condition) {
        for (usize idx : members) {
          auto &entry = entries_[idx];
          entry.system = std::make_unique<ConditionedSystem>(
              std::move(entry.system), cfg.condition);
          entry.access = entry.system->access();
        }
      }

      // CACHING
      auto get_names = [&](SetId set_id) -> std::vector<std::string> {
        std::vector<std::string> names;
        if (auto iter = set_members.find(set_id); iter != set_members.end()) {
          for (usize idx : iter->second) {
            names.push_back(entries_[idx].system->name());
          }
        }
        return names;
      };

      // BEFORE
      for (const auto &target_set : cfg.before) {
        auto target_names = get_names(target_set);
        for (usize from_idx : members) {
          auto &before_list = entries_[from_idx].ordering.before;
          for (const auto &name : target_names) {
            if (name != entries_[from_idx].system->name()) {
              before_list.emplace_back(name);
            }
          }
        }
      }

      // AFTER
      for (const auto &target_set : cfg.after) {
        auto target_names = get_names(target_set);
        for (usize to_idx : members) {
          auto &after_list = entries_[to_idx].ordering.after;
          for (const auto &name : target_names) {
            if (name != entries_[to_idx].system->name()) {
              after_list.emplace_back(name);
            }
          }
        }
      }
    }
  }

  auto topological_sort() -> void {
    const usize n = entries_.size();

    std::unordered_map<std::string, usize> name_to_idx;
    for (usize i = 0; i < n; ++i) {
      name_to_idx[entries_[i].system->name()] = i;
    }

    std::vector<std::vector<usize>> adj(n);
    std::vector<usize> in_degree(n, 0);

    auto resolve = [&](const std::string &dep,
                       const std::string &owner) -> unsigned long {
      auto it = name_to_idx.find(dep);
      if (it == name_to_idx.end())
        throw std::runtime_error("System '" + owner + "' references '" + dep +
                                 "' which does not exist in this schedule");
      return it->second;
    };

    // Deduplicate edges to avoid double-counting in_degree
    auto add_edge = [&](usize from, usize to) -> void {
      if (std::ranges::find(adj[from], to) == adj[from].end()) {
        adj[from].push_back(to);
        in_degree[to]++;
      }
    };

    for (usize i = 0; i < n; ++i) {
      const auto &name = entries_[i].system->name();
      for (const auto &dep : entries_[i].ordering.after) {
        usize j = resolve(dep, name);
        add_edge(j, i);
      }
      for (const auto &dep : entries_[i].ordering.before) {
        usize j = resolve(dep, name);
        add_edge(i, j);
      }
    }

    // Kahn's algorithm, stable by insertion order
    std::vector<usize> ready;
    for (usize i = 0; i < n; ++i) {
      if (in_degree[i] == 0) {
        ready.push_back(i);
      }
    }

    std::vector<usize> order;
    order.reserve(n);
    while (!ready.empty()) {
      auto min_it = std::ranges::min_element(ready);
      usize curr = *min_it;
      ready.erase(min_it);
      order.push_back(curr);

      for (usize next : adj[curr]) {
        if (--in_degree[next] == 0) {
          ready.push_back(next);
        }
      }
    }

    if (order.size() != n) {
      std::string msg = "Cycle detected in system ordering. Systems in cycle:";
      for (usize i = 0; i < n; ++i) {
        if (in_degree[i] != 0) {
          msg += " " + entries_[i].system->name();
        }
      }
      throw std::runtime_error(msg);
    }

    std::vector<SystemEntry> sorted;
    sorted.reserve(n);
    for (usize idx : order) {
      sorted.push_back(std::move(entries_[idx]));
    }
    entries_ = std::move(sorted);
  }

  auto build_execution_steps() -> void {
    usize start = 0;
    while (start < entries_.size()) {
      if (entries_[start].access.exclusive) {
        steps_.emplace_back(InlineStep{.start = start, .end = start + 1});
        ++start;
        continue;
      }

      usize end = start + 1;
      while (end < entries_.size() && !entries_[end].access.exclusive) {
        ++end;
      }

      if (end - start == 1) {
        steps_.emplace_back(InlineStep{.start = start, .end = end});
      } else {
        steps_.emplace_back(build_parallel_step(start, end));
      }

      start = end;
    }
  }

  [[nodiscard]] auto systems_conflict(usize i, usize j) const -> bool {
    const auto &a = entries_[i];
    const auto &b = entries_[j];

    if (a.access.exclusive || b.access.exclusive ||
        !a.access.is_compatible(b.access))
      return true;

    return have_ordering_dependency(a, b);
  }

  using Batch = std::vector<usize>;

  [[nodiscard]] auto batch_compatible_systems(usize start, usize end) const
      -> std::vector<Batch> {
    std::vector<Batch> batches;
    for (usize offset = 0; offset < (end - start); ++offset) {
      auto iter = std::ranges::find_if(batches, [&](const auto &batch) -> auto {
        return std::ranges::none_of(batch, [&](auto member) -> auto {
          return systems_conflict(start + offset, start + member);
        });
      });

      if (iter != batches.end()) {
        iter->push_back(offset);
      } else {
        batches.push_back({offset});
      }
    }
    return batches;
  }

  auto reorder_entries_by_batches(usize start,
                                  const std::vector<Batch> &batches) -> void {
    std::vector<SystemEntry> reordered;
    reordered.reserve(batches.size());

    for (const auto &batch : batches) {
      for (usize idx : batch) {
        reordered.push_back(std::move(entries_[start + idx]));
      }
    }

    for (usize i = 0; i < reordered.size(); ++i) {
      entries_[start + i] = std::move(reordered[i]);
    }
  }

  auto build_parallel_step(usize start, usize end) -> ExecutionStep {
    auto batches = batch_compatible_systems(start, end);
    reorder_entries_by_batches(start, batches);

    bool any_parallel = std::ranges::any_of(
        batches, [](auto &b) -> auto { return b.size() > 1; });
    if (!any_parallel) {
      return InlineStep{.start = start, .end = end};
    }

    ParallelStep step;
    usize batch_start = start;

    for (const auto &batch : batches) {
      usize batch_end = batch_start + batch.size();
      step.batches.push_back({batch_start, batch_end});
      batch_start = batch_end;
    }

    return step;
  }

  auto run_parallel_step(const ParallelStep &step, World &world) -> void {
    for (const auto &batch : step.batches) {
      tf::Taskflow taskflow;
      bool has_worker_tasks = false;

      for (usize idx : range(batch.start, batch.end)) {
        if (entries_[idx].access.main_thread_only) {
          continue;
        }

        has_worker_tasks = true;
        taskflow.emplace(
            [this, idx, &world] -> void { run_entry(idx, world); });
      }

      if (!has_worker_tasks) {
        for (usize idx : range(batch.start, batch.end)) {
          run_entry(idx, world);
        }
        continue;
      }

      auto future = global_executor().run(taskflow);

      for (usize idx : range(batch.start, batch.end)) {
        if (!entries_[idx].access.main_thread_only) {
          continue;
        }
        run_entry(idx, world);
      }

      future.wait();
    }
  }

  auto run_entry(usize index, World &world) -> void {
#ifdef TRACY_ENABLE
    const auto sys_name = entries_[index].system->name();
    ZoneScopedTransient(sys_name.c_str());
#endif
    entries_[index].system->run(world);
  }

  static auto have_ordering_dependency(const SystemEntry &a,
                                       const SystemEntry &b) -> bool {
    const auto &a_name = a.system->name();
    const auto &b_name = b.system->name();

    return std::ranges::contains(a.ordering.after, b_name) ||
           std::ranges::contains(a.ordering.before, b_name) ||
           std::ranges::contains(b.ordering.after, a_name) ||
           std::ranges::contains(b.ordering.before, a_name);
  }
};

class Schedules {
  std::unordered_map<ScheduleLabel, Schedule> schedules_;

public:
  auto entry(ScheduleLabel label) -> Schedule & { return schedules_[label]; }

  auto get(ScheduleLabel label) -> Schedule * {
    auto iter = schedules_.find(label);
    return iter != schedules_.end() ? &iter->second : nullptr;
  }

  auto run(ScheduleLabel label, World &world) -> void {
    auto *schedule = get(label);
    if (schedule != nullptr) {
      ZoneScopedN("Schedule");
#ifdef TRACY_ENABLE
      auto name = schedule_label_name(label);
      ZoneText(name, std::strlen(name));
#endif
      schedule->run(world);
    }
  }

  auto prepare_all() -> void {
    for (auto &[_, schedule] : schedules_) {
      schedule.prepare();
    }
  }

#ifndef NDEBUG
  auto debug_dump() const -> std::string;
#endif

private:
  friend struct ScheduleDebug;
};

namespace detail {

template <typename F>
auto add_system_to_schedule(Schedule &schedule, F &&func) -> void {
  schedule.add_system_fn(std::forward<F>(func));
}

} // namespace detail

inline auto system_multithreading(TaskPoolOptions opts = {}) {
  return [opts](auto &app) -> auto {
    app.world().set_multithreaded(opts.multithreaded);
  };
}

} // namespace ecs

#ifndef NDEBUG
#include "schedule_debug.hpp"

namespace ecs {
inline auto Schedule::debug_dump(const std::string &schedule_name) const
    -> std::string {
  return ScheduleDebug::dump(*this, schedule_name);
}

inline auto Schedules::debug_dump() const -> std::string {
  return ScheduleDebug::dump(*this);
}
} // namespace ecs
#endif
