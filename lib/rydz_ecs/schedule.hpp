#pragma once
#include "condition.hpp"
#include "schedule_label.hpp"
#include "system.hpp"
#include "tracy_plugin.hpp"
#include <algorithm>
#include <memory>
#include <string>
#include <taskflow/taskflow.hpp>
#include <unordered_map>
#include <variant>
#include <vector>

namespace ecs {

inline tf::Executor &global_executor() {
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

template <typename Func>
ChainedSystems::Entry make_chain_entry(Func &&func, const std::string &prev) {
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

  return {std::move(sys), std::move(ordering)};
}

} // namespace detail

template <typename... Fs> ChainedSystems chain(Fs &&...funcs) {
  ChainedSystems result;
  std::string prev_name;

  auto process = [&](auto &&func) {
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

  struct ParallelStep {
    tf::Taskflow taskflow;
  };

  using ExecutionStep = std::variant<InlineStep, ParallelStep>;

  std::vector<SystemEntry> entries_;
  std::vector<ExecutionStep> steps_;
  std::vector<SetConfig> set_configs_;

  World *current_world_ = nullptr;
  bool needs_rebuild_ = true;

public:
  Schedule() = default;
  Schedule(Schedule &&) = default;
  Schedule &operator=(Schedule &&) = default;

  void add_system(std::unique_ptr<ISystem> system,
                  SystemOrdering ordering = {}) {
    SystemEntry entry;
    entry.system = std::move(system);
    entry.access = entry.system->access();
    entry.in_sets = std::move(ordering.in_sets);
    entry.ordering = std::move(ordering);
    entries_.push_back(std::move(entry));
    needs_rebuild_ = true;
  }

  void add_set_config(SetConfig config) {
    set_configs_.push_back(std::move(config));
    needs_rebuild_ = true;
  }

  template <typename F> void add_system_fn(F &&func) {
    add_system_fn_with_ordering(std::forward<F>(func), {});
  }

  template <typename E, typename F>
    requires(std::is_enum_v<bare_t<E>> &&
             !std::same_as<bare_t<E>, ScheduleLabel>)
  void add_system_fn(E set_id, F &&func) {
    add_system_fn(set(set_id), std::forward<F>(func));
  }

  template <typename S, typename F>
    requires(detail::is_set_marker_v<S>)
  void add_system_fn(S &&, F &&func) {
    add_system_fn(set<bare_t<S>>(), std::forward<F>(func));
  }

  template <typename S, typename F>
    requires(std::same_as<bare_t<S>, SetId> || std::same_as<bare_t<S>, SetList>)
  void add_system_fn(S &&selector, F &&func) {
    SystemOrdering ordering;
    ordering.in_sets = detail::to_set_ids(std::forward<S>(selector));
    detail::ensure_unique_sets(ordering.in_sets, "add_system_fn(...)");
    add_system_fn_with_ordering(std::forward<F>(func), std::move(ordering));
  }

  void run(World &world) {
    if (entries_.empty())
      return;

    current_world_ = &world;
    if (needs_rebuild_) {
      rebuild();
      needs_rebuild_ = false;
    }

    if (!world.multithreaded()) {
      for (auto &entry : entries_) {
#ifdef TRACY_ENABLE
        const auto sys_name = entry.system->name();
        ZoneScopedTransient(sys_name.c_str());
#endif
        entry.system->run(world);
      }
      return;
    }

    for (auto &step : steps_) {
      if (auto *s = std::get_if<InlineStep>(&step)) {
        for (usize i : range(s->start, s->end)) {
#ifdef TRACY_ENABLE
          const auto sys_name = entries_[i].system->name();
          ZoneScopedTransient(sys_name.c_str());
#endif
          entries_[i].system->run(world);
        }
      } else {
        auto &tf = std::get<ParallelStep>(step).taskflow;
        global_executor().run(tf).wait();
      }
    }
  }

  usize system_count() const { return entries_.size(); }
  bool empty() const { return entries_.empty(); }

private:
  template <typename F>
  void add_system_fn_with_ordering(F &&func, SystemOrdering base_ordering) {
    if constexpr (is_chained_systems_v<bare_t<F>>) {
      for (auto &e : func.systems) {
        merge_ordering(e.ordering, base_ordering);
        add_system(std::move(e.system), std::move(e.ordering));
      }

    } else if constexpr (is_system_group_descriptor_v<bare_t<F>>) {
      for (auto &e : func.build()) {
        merge_ordering(e.ordering, base_ordering);
        add_system(std::move(e.system), std::move(e.ordering));
      }

    } else if constexpr (is_system_descriptor_v<bare_t<F>>) {
      auto ordering = func.take_ordering();
      merge_ordering(ordering, std::move(base_ordering));
      add_system(func.build(), std::move(ordering));

    } else {
      add_system(make_system(std::forward<F>(func)), std::move(base_ordering));
    }
  }
 
  static void merge_ordering(SystemOrdering &target,
                             const SystemOrdering &extra) {
    target.after.insert(target.after.end(), extra.after.begin(),
                        extra.after.end());
    target.before.insert(target.before.end(), extra.before.begin(),
                         extra.before.end());
    target.in_sets.insert(target.in_sets.end(), extra.in_sets.begin(),
                          extra.in_sets.end());
  }

  void rebuild() {
    steps_.clear();

    const usize n = entries_.size();
    if (n == 0)
      return;

    apply_set_configs();
    topological_sort();
    build_execution_steps();
  }

  void apply_set_configs() {
    std::unordered_map<SetId, std::vector<usize>> set_members;
    for (usize i = 0; i < entries_.size(); ++i)
      for (const auto &sid : entries_[i].in_sets)
        set_members[sid].push_back(i);

    std::unordered_map<SetId, bool> configured_sets;
    for (const auto &cfg : set_configs_) {
      configured_sets[cfg.id] = true;
    }

    for (const auto &[sid, _] : set_members) {
      if (!configured_sets.contains(sid)) {
        throw std::runtime_error("Set '" + set_name(sid) +
                                 "' is used by systems in this schedule but "
                                 "was never configured");
      }
    }

    for (auto &cfg : set_configs_) {
      if (cfg.condition) {
        for (usize idx : set_members[cfg.id]) {
          entries_[idx].system = std::make_unique<ConditionedSystem>(
              std::move(entries_[idx].system), cfg.condition);
        }
      }

      for (const auto &before_set : cfg.before) {
        for (usize from : set_members[cfg.id]) {
          for (usize to : set_members[before_set]) {
            if (from != to)
              entries_[from].ordering.before.push_back(
                  entries_[to].system->name());
          }
        }
      }

      for (const auto &after_set : cfg.after) {
        for (usize to : set_members[cfg.id]) {
          for (usize from : set_members[after_set]) {
            if (from != to)
              entries_[to].ordering.after.push_back(
                  entries_[from].system->name());
          }
        }
      }
    }
  }

  void topological_sort() {
    const usize n = entries_.size();

    std::unordered_map<std::string, usize> name_to_idx;
    for (usize i = 0; i < n; ++i)
      name_to_idx[entries_[i].system->name()] = i;

    std::vector<std::vector<usize>> adj(n);
    std::vector<usize> in_degree(n, 0);

    auto resolve = [&](const std::string &dep, const std::string &owner) {
      auto it = name_to_idx.find(dep);
      if (it == name_to_idx.end())
        throw std::runtime_error("System '" + owner + "' references '" + dep +
                                 "' which does not exist in this schedule");
      return it->second;
    };

    // Deduplicate edges to avoid double-counting in_degree
    auto add_edge = [&](usize from, usize to) {
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
    for (usize i = 0; i < n; ++i)
      if (in_degree[i] == 0)
        ready.push_back(i);

    std::vector<usize> order;
    order.reserve(n);
    while (!ready.empty()) {
      auto min_it = std::min_element(ready.begin(), ready.end());
      usize curr = *min_it;
      ready.erase(min_it);
      order.push_back(curr);

      for (usize next : adj[curr])
        if (--in_degree[next] == 0)
          ready.push_back(next);
    }

    if (order.size() != n) {
      std::string msg = "Cycle detected in system ordering. Systems in cycle:";
      for (usize i = 0; i < n; ++i)
        if (in_degree[i] != 0)
          msg += " " + entries_[i].system->name();
      throw std::runtime_error(msg);
    }

    std::vector<SystemEntry> sorted;
    sorted.reserve(n);
    for (usize idx : order)
      sorted.push_back(std::move(entries_[idx]));
    entries_ = std::move(sorted);
  }

  void build_execution_steps() {
    auto chunks =
        entries_ |
        std::views::chunk_by([](const SystemEntry &a, const SystemEntry &b) {
          return a.access.main_thread_only == b.access.main_thread_only;
        });

    for (auto chunk : chunks) {
      usize start =
          static_cast<usize>(std::distance(entries_.data(), &chunk.front()));
      usize end = start + chunk.size();
      bool is_inline =
          chunk.front().access.main_thread_only || chunk.size() == 1;

      if (is_inline) {
        steps_.emplace_back(InlineStep{start, end});
      } else {
        steps_.emplace_back(build_parallel_step(start, end));
      }
    }
  }

  bool systems_conflict(usize i, usize j) const {
    const auto &a = entries_[i];
    const auto &b = entries_[j];

    if (a.access.exclusive || b.access.exclusive ||
        !a.access.is_compatible(b.access))
      return true;

    return have_ordering_dependency(a, b);
  }

  using Batch = std::vector<usize>;

  std::vector<Batch> batch_compatible_systems(usize start, usize end) const {
    std::vector<Batch> batches;

    const usize count = end - start;
    for (usize i = 0; i < count; ++i) {

      auto compatible = [&](auto &batch) {
        for (usize j : batch) {
          if (systems_conflict(start + i, start + j)) {
            return false;
          }
        }
        return true;
      };

      auto it = std::ranges::find_if(batches, compatible);

      if (it != batches.end()) {
        it->push_back(i);
      } else {
        batches.push_back({i});
      }
    }
    return batches;
  }

  void reorder_entries_by_batches(usize start,
                                  const std::vector<Batch> &batches) {
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

  ExecutionStep build_parallel_step(usize start, usize end) {
    using Tasks = std::vector<tf::Task>;

    auto batches = batch_compatible_systems(start, end);
    reorder_entries_by_batches(start, batches);

    bool any_parallel =
        std::ranges::any_of(batches, [](auto &b) { return b.size() > 1; });
    if (!any_parallel)
      return InlineStep{start, end};

    ParallelStep step;
    usize batch_start = start;
    Tasks prev_tasks;

    for (const auto &batch : batches) {
      Tasks curr_tasks;
      usize batch_end = batch_start + batch.size();

      for (usize i = batch_start; i < batch_end; ++i) {
        auto task = step.taskflow.emplace([this, i] {
#ifdef TRACY_ENABLE
          const auto sys_name = entries_[i].system->name();
          ZoneScopedTransient(sys_name.c_str());
#endif
          entries_[i].system->run(*current_world_);
        });

        for (auto &prev : prev_tasks) {
          prev.precede(task);
        }

        curr_tasks.push_back(task);
      }

      prev_tasks = std::move(curr_tasks);
      batch_start = batch_end;
    }

    return step;
  }

  static bool have_ordering_dependency(const SystemEntry &a,
                                       const SystemEntry &b) {
    const auto &na = a.system->name();
    const auto &nb = b.system->name();

    return std::ranges::contains(a.ordering.after, nb) ||
           std::ranges::contains(a.ordering.before, nb) ||
           std::ranges::contains(b.ordering.after, na) ||
           std::ranges::contains(b.ordering.before, na);
  }
};

class Schedules {
  std::unordered_map<ScheduleLabel, Schedule> schedules_;

public:
  Schedule &entry(ScheduleLabel label) { return schedules_[label]; }

  Schedule *get(ScheduleLabel label) {
    auto it = schedules_.find(label);
    return it != schedules_.end() ? &it->second : nullptr;
  }

  void run(ScheduleLabel label, World &world) {
    auto *schedule = get(label);
    if (schedule) {
      ZoneScopedN("Schedule");
#ifdef TRACY_ENABLE
      auto name = schedule_label_name(label);
      ZoneText(name, std::strlen(name));
#endif
      schedule->run(world);
    }
  }
};

namespace detail {

template <typename F>
void add_system_to_schedule(Schedule &schedule, F &&func) {
  schedule.add_system_fn(std::forward<F>(func));
}

} // namespace detail

inline auto system_multithreading(TaskPoolOptions opts = {}) {
  return
      [opts](auto &app) { app.world().set_multithreaded(opts.multithreaded); };
}

} // namespace ecs
