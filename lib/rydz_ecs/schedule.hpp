#pragma once
#include "condition.hpp"
#include "system.hpp"
#include <absl/container/flat_hash_map.h>
#include <functional>
#include <memory>
#include <string>
#include <taskflow/taskflow.hpp>
#include <vector>

using namespace std;

namespace ecs {

enum class ScheduleLabel {
  PreStartup,
  Startup,
  PostStartup,
  First,
  PreUpdate,
  Update,
  PostUpdate,
  Last,
  FixedUpdate,
};

} // namespace ecs

template <> struct std::hash<ecs::ScheduleLabel> {
  size_t operator()(ecs::ScheduleLabel l) const noexcept {
    return std::hash<int>{}(static_cast<int>(l));
  }
};

namespace ecs {

inline tf::Executor &global_executor() {
  static tf::Executor executor;
  return executor;
}

class Schedule {
  struct SystemEntry {
    std::unique_ptr<ISystem> system;
    SystemAccess access;
  };

  enum class SegmentKind {
    Inline,
    Parallel,
  };

  struct Segment {
    size_t start = 0;
    size_t end = 0;
    SegmentKind kind = SegmentKind::Inline;
    tf::Taskflow taskflow;
  };

  std::vector<SystemEntry> entries_;
  std::vector<Segment> segments_;
  World *current_world_ = nullptr;
  bool needs_rebuild_ = true;

public:
  Schedule() = default;
  Schedule(Schedule &&) = default;
  Schedule &operator=(Schedule &&) = default;

  void add_system(std::unique_ptr<ISystem> system) {
    SystemEntry entry{std::move(system), {}};
    entry.access = entry.system->access();
    entries_.push_back(std::move(entry));
    needs_rebuild_ = true;
  }

  template <typename F> void add_system_fn(F &&func) {
    if constexpr (is_system_descriptor_v<std::decay_t<F>>) {
      add_system(func.build());
    } else {
      add_system(make_system(std::forward<F>(func)));
    }
  }

  template <typename F, typename Cond>
  void add_system_fn_if(F &&func, Cond &&cond) {
    add_system(make_system_run_if(std::forward(func), std::forward(cond)));
  }

  void run(World &world) {
    if (entries_.empty())
      return;

    current_world_ = &world;
    if (needs_rebuild_) {
      rebuild_graph();
      needs_rebuild_ = false;
    }

    for (auto &segment : segments_) {
      if (segment.kind == SegmentKind::Inline) {
        for (size_t i : range(segment.start, segment.end))
          entries_[i].system->run(world);
      } else {
        global_executor().run(segment.taskflow).wait();
      }
    }
  }

  size_t system_count() const { return entries_.size(); }
  bool empty() const { return entries_.empty(); }

private:
  void rebuild_graph() {
    segments_.clear();

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
        segments_.push_back({start, end, SegmentKind::Inline, {}});
      } else {
        Segment segment{start, end, SegmentKind::Parallel, {}};
        build_parallel_segment(segment);
        segments_.push_back(std::move(segment));
      }
    }
  }

  void build_parallel_segment(Segment &segment) {
    segment.taskflow.clear();
    const size_t count = segment.end - segment.start;
    std::vector<tf::Task> tasks{};
    tasks.reserve(count);

    for (size_t i : range(segment.start, segment.end)) {
      tasks.push_back(segment.taskflow.emplace(
          [this, i] { entries_[i].system->run(*current_world_); }));
    }

    for (size_t j : range(1u, count)) {
      for (size_t i : range(0u, j)) {
        const auto &a = entries_[segment.start + i].access;
        const auto &b = entries_[segment.start + j].access;
        if (a.exclusive || b.exclusive || !a.is_compatible(b))
          tasks[i].precede(tasks[j]);
      }
    }
  }
};

class Schedules {
  absl::flat_hash_map<ScheduleLabel, Schedule, std::hash<ScheduleLabel>>
      schedules_;

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
  if constexpr (is_system_descriptor_v<std::decay_t<F>>) {
    schedule.add_system(func.build());
  } else {
    schedule.add_system_fn(std::forward<F>(func));
  }
}

} // namespace detail

} // namespace ecs
