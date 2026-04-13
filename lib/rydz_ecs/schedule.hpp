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

  usize system_count() const { return entries_.size(); }
  usize configured_set_count() const { return set_configs_.size(); }
  bool empty() const { return entries_.empty(); }

  void prepare() {
    if (needs_rebuild_) {
      rebuild();
      needs_rebuild_ = false;
    }
  }

#ifndef NDEBUG
  std::string debug_dump(const std::string &schedule_name) const {
    std::ostringstream out;

    std::unordered_map<SetId, usize> set_counts;
    for (const auto &entry : entries_) {
      for (const auto &set_id : entry.in_sets) {
        set_counts[set_id]++;
      }
    }

    out << "=======================\n";
    out << "SCHEDULE " << schedule_name << ":\n";
    out << "  COUNTS: systems=" << entries_.size()
        << ", configured_sets=" << set_configs_.size()
        << ", steps=" << steps_.size()
        << ", max_concurrency=" << debug_max_concurrency() << "\n";

    if (!steps_.empty()) {
      out << "  EXECUTION:\n";
      usize step_index = 0;
      for (const auto &step : steps_) {
        out << "    step[" << step_index << "]:\n";
        out << debug_step_summary(step);
        step_index++;
      }
    }

    if (!set_configs_.empty()) {
      out << "  SETS:\n";
      for (const auto &cfg : set_configs_) {
        out << "    - " << set_name(cfg.id) << "\n";
        out << "      MEMBERS: " << set_counts[cfg.id] << "\n";
        debug_append_names(out, "      ORDER", ordered_systems_in_set(cfg.id),
                           "        ");
        out << "      RUN_IF: " << (cfg.condition ? "yes" : "no") << "\n";
      }
    }

    if (!entries_.empty()) {
      auto placements = debug_system_placements();
      out << "  SYSTEMS:\n";
      for (usize i = 0; i < entries_.size(); ++i) {
        const auto &entry = entries_[i];
        out << debug_item("    - ", entry.system->name());
        auto step =
            i < placements.size() ? std::to_string(placements[i].step) : "?";
        auto placement =
            i < placements.size() ? placements[i].placement : "unknown";
        debug_append_system_metadata(out, i, entry, step, placement);
      }
    }

    return out.str();
  }
#endif

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
          entries_[idx].access = entries_[idx].system->access();
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
    usize start = 0;
    while (start < entries_.size()) {
      if (entries_[start].access.exclusive) {
        steps_.emplace_back(InlineStep{start, start + 1});
        ++start;
        continue;
      }

      usize end = start + 1;
      while (end < entries_.size() && !entries_[end].access.exclusive) {
        ++end;
      }

      if (end - start == 1) {
        steps_.emplace_back(InlineStep{start, end});
      } else {
        steps_.emplace_back(build_parallel_step(start, end));
      }

      start = end;
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
    auto batches = batch_compatible_systems(start, end);
    reorder_entries_by_batches(start, batches);

    bool any_parallel =
        std::ranges::any_of(batches, [](auto &b) { return b.size() > 1; });
    if (!any_parallel)
      return InlineStep{start, end};

    ParallelStep step;
    usize batch_start = start;

    for (const auto &batch : batches) {
      usize batch_end = batch_start + batch.size();
      step.batches.push_back({batch_start, batch_end});
      batch_start = batch_end;
    }

    return step;
  }

  void run_parallel_step(const ParallelStep &step, World &world) {
    for (const auto &batch : step.batches) {
      tf::Taskflow taskflow;
      bool has_worker_tasks = false;

      for (usize i : range(batch.start, batch.end)) {
        if (entries_[i].access.main_thread_only) {
          continue;
        }

        has_worker_tasks = true;
        taskflow.emplace([this, i, &world] { run_entry(i, world); });
      }

      if (!has_worker_tasks) {
        for (usize i : range(batch.start, batch.end)) {
          run_entry(i, world);
        }
        continue;
      }

      auto future = global_executor().run(taskflow);

      for (usize i : range(batch.start, batch.end)) {
        if (!entries_[i].access.main_thread_only) {
          continue;
        }
        run_entry(i, world);
      }

      future.wait();
    }
  }

  void run_entry(usize index, World &world) {
#ifdef TRACY_ENABLE
    const auto sys_name = entries_[index].system->name();
    ZoneScopedTransient(sys_name.c_str());
#endif
    entries_[index].system->run(world);
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

#ifndef NDEBUG
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

  usize debug_max_concurrency() const {
    usize max_concurrency = entries_.empty() ? 0 : 1;
    for (const auto &step : steps_) {
      if (const auto *parallel = std::get_if<ParallelStep>(&step)) {
        for (const auto &batch : parallel->batches) {
          max_concurrency = std::max(max_concurrency, batch.end - batch.start);
        }
      }
    }
    return max_concurrency;
  }

  std::string debug_step_summary(const ExecutionStep &step) const {
    if (const auto *inline_step = std::get_if<InlineStep>(&step)) {
      std::ostringstream out;
      out << "      MODE: inline\n";
      out << "      THREADS: 1\n";
      out << "      SYSTEMS: " << (inline_step->end - inline_step->start)
          << "\n";
      out << "      MEMBERS:\n";
      for (usize i : range(inline_step->start, inline_step->end)) {
        out << debug_item("        - ", entries_[i].system->name());
      }
      return out.str();
    }

    const auto &parallel = std::get<ParallelStep>(step);
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
      for (usize i : range(batch.start, batch.end)) {
        out << debug_item("          - ", entries_[i].system->name());
      }
    }
    return out.str();
  }

  std::vector<DebugPlacement> debug_system_placements() const {
    std::vector<DebugPlacement> placements(entries_.size());

    for (usize step_index = 0; step_index < steps_.size(); ++step_index) {
      const auto &step = steps_[step_index];
      if (const auto *inline_step = std::get_if<InlineStep>(&step)) {
        for (usize i : range(inline_step->start, inline_step->end)) {
          placements[i] = DebugPlacement{step_index, "main"};
        }
        continue;
      }

      const auto &parallel = std::get<ParallelStep>(step);
      for (usize batch_index = 0; batch_index < parallel.batches.size();
           ++batch_index) {
        const auto &batch = parallel.batches[batch_index];
        usize slot = 0;
        for (usize i : range(batch.start, batch.end)) {
          if (entries_[i].access.main_thread_only) {
            placements[i] = DebugPlacement{
                step_index,
                "main_thread(batch=" + std::to_string(batch_index) + ")"};
            continue;
          }

          placements[i] = DebugPlacement{
              step_index, "worker_pool(batch=" + std::to_string(batch_index) +
                              ", slot=" + std::to_string(slot) + ")"};
          slot++;
        }
      }
    }

    return placements;
  }

  static void debug_append_names(std::ostringstream &out, const char *label,
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

  static void debug_append_set_ids(std::ostringstream &out, const char *label,
                                   const std::vector<SetId> &sets,
                                   const char *indent) {
    if (sets.empty()) {
      out << label << ": none\n";
      return;
    }

    out << label << ":\n";
    for (const auto &set_id : sets) {
      out << indent << "- " << set_name(set_id) << "\n";
    }
  }

  std::vector<std::string> ordered_systems_in_set(const SetId &set_id) const {
    std::vector<std::string> systems;
    for (const auto &entry : entries_) {
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

  static void debug_append_system_metadata(std::ostringstream &out, usize order,
                                           const SystemEntry &entry,
                                           const std::string &step,
                                           const std::string &placement) {
    out << "      ORDER: " << order
        << ", SETS: " << inline_set_ids(entry.in_sets) << ", MAIN_THREAD_ONLY: "
        << (entry.access.main_thread_only ? "yes" : "no")
        << ", EXCLUSIVE: " << (entry.access.exclusive ? "yes" : "no")
        << ", STEP: " << step << ", PLACEMENT: " << placement << "\n";
  }
#endif
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

  void prepare_all() {
    for (auto &[_, schedule] : schedules_) {
      schedule.prepare();
    }
  }

#ifndef NDEBUG
  std::string debug_dump() const {
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
    for (const auto &[_, schedule] : schedules_) {
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
      auto it = schedules_.find(label);
      if (it == schedules_.end()) {
        continue;
      }

      const auto &schedule = it->second;
      out << schedule.debug_dump(schedule_label_name(label));
    }
    return out.str();
  }
#endif
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
