#pragma once
#include "condition.hpp"
#include "system.hpp"
#include <absl/container/flat_hash_map.h>
#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <taskflow/taskflow.hpp>
#include <vector>

namespace ecs {

enum class ScheduleLabel {
  PreStartup,
  Startup,
  PostStartup,
  First,
  PreUpdate,
  Update,
  PostUpdate,
  ExtractRender,
  Render,
  PostRender,
  Last,
  FixedUpdate,
};

template <typename H> H AbslHashValue(H h, ScheduleLabel l) {
  return H::combine(std::move(h), static_cast<i32>(l));
}

} // namespace ecs

template <> struct std::hash<ecs::ScheduleLabel> {
  size_t operator()(ecs::ScheduleLabel l) const noexcept {
    return std::hash<i32>{}(static_cast<i32>(l));
  }
};

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

  if constexpr (is_system_descriptor_v<std::decay_t<Func>>) {
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
  };

  std::vector<SystemEntry> entries_;

  // Inline-only segments for main_thread_only systems
  struct InlineSegment {
    size_t start;
    size_t end;
  };

  struct TaskflowSegment {
    tf::Taskflow taskflow;
  };

  enum class StepKind { Inline, Taskflow };
  struct Step {
    StepKind kind;
    size_t idx;
  };

  std::vector<InlineSegment> inline_segments_;
  std::vector<TaskflowSegment> taskflow_segments_;
  std::vector<Step> execution_plan_;

  World *current_world_ = nullptr;
  bool needs_rebuild_ = true;
  bool has_parallel_ = false;

public:
  Schedule() = default;
  Schedule(Schedule &&) = default;
  Schedule &operator=(Schedule &&) = default;

  void add_system(std::unique_ptr<ISystem> system,
                  SystemOrdering ordering = {}) {
    SystemEntry entry{std::move(system), {}, std::move(ordering)};
    entry.access = entry.system->access();
    entries_.push_back(std::move(entry));
    needs_rebuild_ = true;
  }

  template <typename F> void add_system_fn(F &&func) {
    if constexpr (is_chained_systems_v<std::decay_t<F>>) {
      for (auto &e : func.systems) {
        add_system(std::move(e.system), std::move(e.ordering));
      }
    } else if constexpr (is_system_group_descriptor_v<std::decay_t<F>>) {
      for (auto &e : func.build()) {
        add_system(std::move(e.system), std::move(e.ordering));
      }
    } else if constexpr (is_system_descriptor_v<std::decay_t<F>>) {
      auto ordering = func.take_ordering();
      add_system(func.build(), std::move(ordering));
    } else {
      add_system(make_system(std::forward<F>(func)));
    }
  }

  void run(World &world) {
    if (entries_.empty())
      return;

    current_world_ = &world;
    if (needs_rebuild_) {
      rebuild_graph();
      needs_rebuild_ = false;
    }

    if (!has_parallel_ || !world.multithreaded()) {
      for (auto &entry : entries_)
        entry.system->run(world);
      return;
    }

    for (auto &step : execution_plan_) {
      if (step.kind == StepKind::Inline) {
        auto &seg = inline_segments_[step.idx];
        for (size_t i : range(seg.start, seg.end))
          entries_[i].system->run(world);
      } else {
        global_executor().run(taskflow_segments_[step.idx].taskflow).wait();
      }
    }
  }

  size_t system_count() const { return entries_.size(); }
  bool empty() const { return entries_.empty(); }

private:
  void rebuild_graph() {
    inline_segments_.clear();
    taskflow_segments_.clear();
    execution_plan_.clear();
    has_parallel_ = false;

    const size_t n = entries_.size();
    if (n == 0)
      return;

    absl::flat_hash_map<std::string, size_t> name_to_index;
    for (size_t i = 0; i < n; ++i) {
      name_to_index[entries_[i].system->name()] = i;
    }

    std::vector<std::vector<size_t>> adj(n);
    std::vector<size_t> in_degree(n, 0);

    for (size_t i = 0; i < n; ++i) {
      for (const auto &dep : entries_[i].ordering.after) {
        auto it = name_to_index.find(dep);
        if (it == name_to_index.end()) {
          throw std::runtime_error("System '" + entries_[i].system->name() +
                                   "' has .after() on '" + dep +
                                   "' which does not exist in this schedule");
        }
        adj[it->second].push_back(i);
        in_degree[i]++;
      }
      for (const auto &dep : entries_[i].ordering.before) {
        auto it = name_to_index.find(dep);
        if (it == name_to_index.end()) {
          throw std::runtime_error("System '" + entries_[i].system->name() +
                                   "' has .before() on '" + dep +
                                   "' which does not exist in this schedule");
        }
        adj[i].push_back(it->second);
        in_degree[it->second]++;
      }
    }

    // Kahn's algorithm, stable by insertion order
    std::vector<size_t> ready;
    for (size_t i = 0; i < n; ++i) {
      if (in_degree[i] == 0)
        ready.push_back(i);
    }

    std::vector<size_t> order;
    order.reserve(n);
    while (!ready.empty()) {
      auto min_it = std::min_element(ready.begin(), ready.end());
      size_t curr = *min_it;
      ready.erase(min_it);
      order.push_back(curr);

      for (size_t next : adj[curr]) {
        if (--in_degree[next] == 0) {
          ready.push_back(next);
        }
      }
    }

    if (order.size() != n) {
      std::string msg = "Cycle detected in system ordering. Systems in cycle:";
      for (size_t i = 0; i < n; ++i) {
        if (in_degree[i] != 0)
          msg += " " + entries_[i].system->name();
      }
      throw std::runtime_error(msg);
    }

    // Reorder entries per topo sort
    std::vector<SystemEntry> sorted;
    sorted.reserve(n);
    for (size_t idx : order) {
      sorted.push_back(std::move(entries_[idx]));
    }
    entries_ = std::move(sorted);

    // Chunk by main_thread_only, then batch within each chunk
    auto chunked_view =
        entries_ |
        std::views::chunk_by([](const SystemEntry &a, const SystemEntry &b) {
          return a.access.main_thread_only == b.access.main_thread_only;
        });

    for (auto chunk : chunked_view) {
      size_t start =
          static_cast<size_t>(std::distance(entries_.data(), &chunk.front()));

      size_t end = start + chunk.size();

      bool is_main = chunk.front().access.main_thread_only;

      if (is_main || (chunk.size() == 1)) {
        size_t idx = inline_segments_.size();
        inline_segments_.push_back({start, end});
        execution_plan_.push_back({StepKind::Inline, idx});
      } else {
        build_taskflow_segment(start, end);
      }
    }
  }

  bool systems_conflict(size_t i, size_t j) const {
    const auto &a = entries_[i];
    const auto &b = entries_[j];

    if (a.access.exclusive || b.access.exclusive ||
        !a.access.is_compatible(b.access))
      return true;

    const auto &name_a = a.system->name();
    const auto &name_b = b.system->name();
    for (const auto &dep : a.ordering.after)
      if (dep == name_b)
        return true;
    for (const auto &dep : a.ordering.before)
      if (dep == name_b)
        return true;
    for (const auto &dep : b.ordering.after)
      if (dep == name_a)
        return true;
    for (const auto &dep : b.ordering.before)
      if (dep == name_a)
        return true;

    return false;
  }

  void build_taskflow_segment(size_t start, size_t end) {
    const size_t count = end - start;

    // Greedy batch assignment: each batch holds mutually compatible systems
    std::vector<std::vector<size_t>> batches;

    for (size_t i = 0; i < count; ++i) {
      bool placed = false;
      for (auto &batch : batches) {
        bool compatible = true;
        for (size_t j : batch) {
          if (systems_conflict(start + i, start + j)) {
            compatible = false;
            break;
          }
        }
        if (compatible) {
          batch.push_back(i);
          placed = true;
          break;
        }
      }
      if (!placed) {
        batches.push_back({i});
      }
    }

    // Reorder entries within [start, end) by batch
    std::vector<SystemEntry> reordered;
    reordered.reserve(count);
    for (const auto &batch : batches) {
      for (size_t idx : batch) {
        reordered.push_back(std::move(entries_[start + idx]));
      }
    }
    for (size_t i = 0; i < count; ++i) {
      entries_[start + i] = std::move(reordered[i]);
    }

    // Build tasks in a dedicated taskflow with dependency edges between batches
    size_t tf_idx = taskflow_segments_.size();
    taskflow_segments_.emplace_back();
    auto &tf = taskflow_segments_.back().taskflow;

    size_t batch_start = start;
    std::vector<tf::Task> prev_batch_tasks;

    for (const auto &batch : batches) {
      size_t batch_end = batch_start + batch.size();
      std::vector<tf::Task> current_tasks;

      for (size_t i = batch_start; i < batch_end; ++i) {
        auto task =
            tf.emplace([this, i] { entries_[i].system->run(*current_world_); });
        // Each task in this batch depends on all tasks from the previous batch
        for (auto &prev : prev_batch_tasks) {
          prev.precede(task);
        }
        current_tasks.push_back(task);
      }

      prev_batch_tasks = std::move(current_tasks);
      batch_start = batch_end;
    }

    has_parallel_ = true;
    execution_plan_.push_back({StepKind::Taskflow, tf_idx});
  }
};

class Schedules {
  absl::flat_hash_map<ScheduleLabel, Schedule> schedules_;

public:
  Schedule &entry(ScheduleLabel label) { return schedules_[label]; }

  Schedule *get(ScheduleLabel label) {
    auto it = schedules_.find(label);
    return it != schedules_.end() ? &it->second : nullptr;
  }

  void run(ScheduleLabel label, World &world) {
    auto *schedule = get(label);
    if (schedule)
      schedule->run(world);
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
