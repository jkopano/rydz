#pragma once
#include "condition.hpp"
#include "system.hpp"
#include <functional>
#include <memory>
#include <string>
#include <taskflow/taskflow.hpp>
#include <unordered_map>
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
  std::vector<std::unique_ptr<ISystem>> systems_;

public:
  Schedule() = default;

  Schedule(Schedule &&) = default;
  Schedule &operator=(Schedule &&) = default;

  void add_system(std::unique_ptr<ISystem> system) {
    systems_.push_back(std::move(system));
  }

  template <typename F> void add_system_fn(F &&func) {
    systems_.push_back(make_system(std::forward<F>(func)));
  }

  template <typename F, typename Cond>
  void add_system_fn_if(F &&func, Cond &&cond) {
    systems_.push_back(
        make_system_run_if(std::forward<F>(func), std::forward<Cond>(cond)));
  }

  void run(World &world) {
    const size_t n = systems_.size();
    if (n == 0)
      return;

    if (n == 1) {
      systems_[0]->run(world);
      return;
    }

    std::vector<SystemAccess> accesses;
    accesses.reserve(n);
    for (auto &sys : systems_) {
      accesses.push_back(sys->access());
    }

    tf::Taskflow taskflow;
    std::vector<tf::Task> tasks;
    tasks.reserve(n);

    for (size_t i = 0; i < n; ++i) {
      ISystem *sys_ptr = systems_[i].get();
      tasks.push_back(
          taskflow.emplace([sys_ptr, &world]() { sys_ptr->run(world); }));
    }

    for (size_t j = 1; j < n; ++j) {
      if (accesses[j].exclusive) {
        for (size_t i = 0; i < j; ++i) {
          tasks[i].precede(tasks[j]);
        }
        continue;
      }

      for (size_t i = 0; i < j; ++i) {
        if (accesses[i].exclusive) {
          tasks[i].precede(tasks[j]);
          continue;
        }

        if (!accesses[i].is_compatible(accesses[j])) {
          tasks[i].precede(tasks[j]);
        }
      }
    }

    global_executor().run(taskflow).wait();
  }

  size_t system_count() const { return systems_.size(); }
  bool empty() const { return systems_.empty(); }
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

} // namespace ecs
