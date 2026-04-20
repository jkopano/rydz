#pragma once
#include "schedule.hpp"
#include <algorithm>
#include <ranges>
#include <sstream>
#include <string>
#include <vector>

namespace ecs {

#ifndef NDEBUG

struct ScheduleDebug {
  struct DebugPlacement {
    usize step = 0;
    std::string placement;
  };

  static std::string wrap_commas(const std::string &text,
                                 const std::string &continuation_indent) {
    std::string result;
    result.reserve(text.size());

    for (usize i = 0; i < text.size(); ++i) {
      result.push_back(text[i]);
      if (text[i] == ',' && i + 1 < text.size() && text[i + 1] == ' ') {
        result.push_back('\n');
        result += continuation_indent;
        ++i;
      }
    }

    return result;
  }

  static std::string debug_item(const std::string &prefix,
                                const std::string &text) {
    return prefix + wrap_commas(text, std::string(prefix.size(), ' ')) + "\n";
  }

  static usize max_concurrency(const Schedule &schedule) {
    usize max_concurrency = schedule.entries_.empty() ? 0 : 1;
    for (const auto &step : schedule.steps_) {
      if (const auto *parallel = std::get_if<Schedule::ParallelStep>(&step)) {
        for (const auto &batch : parallel->batches) {
          max_concurrency = std::max(max_concurrency, batch.end - batch.start);
        }
      }
    }
    return max_concurrency;
  }

  static std::string step_summary(const Schedule &schedule,
                                  const Schedule::ExecutionStep &step) {
    if (const auto *inline_step = std::get_if<Schedule::InlineStep>(&step)) {
      std::ostringstream out;
      out << "      MODE: inline\n";
      out << "      THREADS: 1\n";
      out << "      SYSTEMS: " << (inline_step->end - inline_step->start)
          << "\n";
      out << "      MEMBERS:\n";
      for (usize idx : range(inline_step->start, inline_step->end)) {
        out << debug_item("        - ", schedule.entries_[idx].system->name());
      }
      return out.str();
    }

    const auto &parallel = std::get<Schedule::ParallelStep>(step);
    std::ostringstream out;
    usize max_concurrency = 0;
    usize system_count = 0;
    for (const auto &batch : parallel.batches) {
      max_concurrency = std::max(max_concurrency, batch.end - batch.start);
      system_count += batch.end - batch.start;
    }

    out << "      MODE: parallel\n";
    out << "      MAX_CONCURRENCY: " << max_concurrency << "\n";
    out << "      SYSTEMS: " << system_count << "\n";
    out << "      BATCHES:\n";
    for (usize batch_index = 0; batch_index < parallel.batches.size();
         ++batch_index) {
      const auto &batch = parallel.batches[batch_index];
      out << "        BATCH[" << batch_index
          << "] SIZE=" << (batch.end - batch.start) << "\n";
      for (usize idx : range(batch.start, batch.end)) {
        out << debug_item("          - ", schedule.entries_[idx].system->name());
      }
    }
    return out.str();
  }

  static std::vector<DebugPlacement>
  system_placements(const Schedule &schedule) {
    std::vector<DebugPlacement> placements(schedule.entries_.size());

    for (usize step_index = 0; step_index < schedule.steps_.size();
         ++step_index) {
      const auto &step = schedule.steps_[step_index];
      if (const auto *inline_step = std::get_if<Schedule::InlineStep>(&step)) {
        for (usize idx : range(inline_step->start, inline_step->end)) {
          placements[idx] =
              DebugPlacement{.step = step_index, .placement = "main"};
        }
        continue;
      }

      const auto &parallel = std::get<Schedule::ParallelStep>(step);
      for (usize batch_index = 0; batch_index < parallel.batches.size();
           ++batch_index) {
        const auto &batch = parallel.batches[batch_index];
        usize slot = 0;
        for (usize idx : range(batch.start, batch.end)) {
          if (schedule.entries_[idx].access.main_thread_only) {
            placements[idx] = DebugPlacement{
                .step = step_index,
                .placement =
                    "main_thread(batch=" + std::to_string(batch_index) + ")"};
            continue;
          }

          placements[idx] = DebugPlacement{
              .step = step_index,
              .placement = "worker_pool(batch=" + std::to_string(batch_index) +
                           ", slot=" + std::to_string(slot) + ")"};
          slot++;
        }
      }
    }

    return placements;
  }

  static void append_names(std::ostringstream &out, const char *label,
                           const std::vector<std::string> &names,
                           const char *indent) {
    if (names.empty()) {
      out << label << ": none\n";
      return;
    }

    out << label << ":\n";
    for (const auto &name : names) {
      out << debug_item(std::string(indent) + "- ", name);
    }
  }

  static std::vector<std::string>
  ordered_systems_in_set(const Schedule &schedule, const SetId &set_id) {
    std::vector<std::string> systems;
    for (const auto &entry : schedule.entries_) {
      if (std::ranges::contains(entry.in_sets, set_id)) {
        systems.push_back(entry.system->name());
      }
    }
    return systems;
  }

  static std::string inline_set_ids(const std::vector<SetId> &sets) {
    if (sets.empty()) {
      return "none";
    }

    std::ostringstream out;
    for (usize i = 0; i < sets.size(); ++i) {
      if (i != 0) {
        out << ", ";
      }
      out << set_name(sets[i]);
    }
    return out.str();
  }

  static void append_system_metadata(std::ostringstream &out, usize order,
                                     const Schedule::SystemEntry &entry,
                                     const std::string &step,
                                     const std::string &placement) {
    out << "      ORDER: " << order
        << ", SETS: " << inline_set_ids(entry.in_sets) << ", MAIN_THREAD_ONLY: "
        << (entry.access.main_thread_only ? "yes" : "no")
        << ", EXCLUSIVE: " << (entry.access.exclusive ? "yes" : "no")
        << ", STEP: " << step << ", PLACEMENT: " << placement << "\n";
  }

  static std::string dump(const Schedule &schedule,
                          const std::string &schedule_name) {
    std::ostringstream out;

    std::unordered_map<SetId, usize> set_counts;
    for (const auto &entry : schedule.entries_) {
      for (const auto &set_id : entry.in_sets) {
        set_counts[set_id]++;
      }
    }

    out << "=======================\n";
    out << "SCHEDULE " << schedule_name << ":\n";
    out << "  COUNTS: systems=" << schedule.entries_.size()
        << ", configured_sets=" << schedule.set_configs_.size()
        << ", steps=" << schedule.steps_.size()
        << ", max_concurrency=" << max_concurrency(schedule) << "\n";

    if (!schedule.steps_.empty()) {
      out << "  EXECUTION:\n";
      usize step_index = 0;
      for (const auto &step : schedule.steps_) {
        out << "    step[" << step_index << "]:\n";
        out << step_summary(schedule, step);
        step_index++;
      }
    }

    if (!schedule.set_configs_.empty()) {
      out << "  SETS:\n";
      for (const auto &cfg : schedule.set_configs_) {
        out << "    - " << set_name(cfg.id) << "\n";
        out << "      MEMBERS: " << set_counts[cfg.id] << "\n";
        append_names(out, "      ORDER",
                     ordered_systems_in_set(schedule, cfg.id), "        ");
        out << "      RUN_IF: " << (cfg.condition ? "yes" : "no") << "\n";
      }
    }

    if (!schedule.entries_.empty()) {
      auto placements = system_placements(schedule);
      out << "  SYSTEMS:\n";
      for (usize i = 0; i < schedule.entries_.size(); ++i) {
        const auto &entry = schedule.entries_[i];
        out << debug_item("    - ", entry.system->name());
        auto step =
            i < placements.size() ? std::to_string(placements[i].step) : "?";
        auto placement =
            i < placements.size() ? placements[i].placement : "unknown";
        append_system_metadata(out, i, entry, step, placement);
      }
    }

    return out.str();
  }

  static std::string dump(const Schedules &schedules) {
    constexpr std::array labels{
        ScheduleLabel::PreStartup,  ScheduleLabel::Startup,
        ScheduleLabel::PostStartup, ScheduleLabel::First,
        ScheduleLabel::PreUpdate,   ScheduleLabel::Update,
        ScheduleLabel::PostUpdate,  ScheduleLabel::ExtractRender,
        ScheduleLabel::Render,      ScheduleLabel::PostRender,
        ScheduleLabel::Last,        ScheduleLabel::FixedUpdate,
    };

    usize total_schedules = 0;
    usize total_systems = 0;
    usize total_sets = 0;
    for (const auto &[_, schedule] : schedules.schedules_) {
      total_schedules++;
      total_systems += schedule.system_count();
      total_sets += schedule.configured_set_count();
    }

    std::ostringstream out;
    out << "[ecs] main schedule graph\n";
    out << "totals: schedules=" << total_schedules
        << ", systems=" << total_systems << ", configured_sets=" << total_sets
        << "\n";

    for (auto label : labels) {
      auto iter = schedules.schedules_.find(label);
      if (iter == schedules.schedules_.end()) {
        continue;
      }

      const auto &schedule = iter->second;
      out << dump(schedule, schedule_label_name(label));
    }
    return out.str();
  }
};

#endif

} // namespace ecs
