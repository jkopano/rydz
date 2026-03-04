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
    if constexpr (is_system_descriptor_v<std::decay_t<F>>) {
      systems_.push_back(func.build());
    } else {
      systems_.push_back(make_system(std::forward<F>(func)));
    }
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

    std::vector<SystemAccess> accesses;
    accesses.reserve(n);
    bool any_main_thread_only = false;
    for (auto &sys : systems_) {
      accesses.push_back(sys->access());
      if (accesses.back().main_thread_only)
        any_main_thread_only = true;
    }

    auto run_segment = [&](size_t start, size_t end) {
      const size_t count = end - start;
      if (count == 0)
        return;
      if (count == 1) {
        systems_[start]->run(world);
        return;
      }

      tf::Taskflow taskflow;
      std::vector<tf::Task> tasks;
      tasks.reserve(count);

      for (size_t i = 0; i < count; ++i) {
        ISystem *sys_ptr = systems_[start + i].get();
        tasks.push_back(
            taskflow.emplace([sys_ptr, &world]() { sys_ptr->run(world); }));
      }

      for (size_t j = 1; j < count; ++j) {
        const auto &acc_j = accesses[start + j];
        if (acc_j.exclusive) {
          for (size_t i = 0; i < j; ++i) {
            tasks[i].precede(tasks[j]);
          }
          continue;
        }

        for (size_t i = 0; i < j; ++i) {
          const auto &acc_i = accesses[start + i];
          if (acc_i.exclusive || !acc_i.is_compatible(acc_j)) {
            tasks[i].precede(tasks[j]);
          }
        }
      }

      global_executor().run(taskflow).wait();
    };

    if (!any_main_thread_only) {
      run_segment(0, n);
      return;
    }

    size_t i = 0;
    while (i < n) {
      if (accesses[i].main_thread_only) {
        systems_[i]->run(world);
        ++i;
        continue;
      }

      size_t j = i + 1;
      while (j < n && !accesses[j].main_thread_only) {
        ++j;
      }
      run_segment(i, j);
      i = j;
    }
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
  if constexpr (is_system_descriptor_v<std::decay_t<F>>) {
    schedule.add_system(func.build());
  } else {
    schedule.add_system_fn(std::forward<F>(func));
  }
}

} // namespace detail

} // namespace ecs
