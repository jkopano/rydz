#pragma once
#include "types.hpp"
#include <cstring>
#include <functional>

namespace ecs {

enum struct ScheduleLabel {
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

inline constexpr ScheduleLabel PreStartup = ScheduleLabel::PreStartup;
inline constexpr ScheduleLabel Startup = ScheduleLabel::Startup;
inline constexpr ScheduleLabel PostStartup = ScheduleLabel::PostStartup;
inline constexpr ScheduleLabel First = ScheduleLabel::First;
inline constexpr ScheduleLabel PreUpdate = ScheduleLabel::PreUpdate;
inline constexpr ScheduleLabel Update = ScheduleLabel::Update;
inline constexpr ScheduleLabel PostUpdate = ScheduleLabel::PostUpdate;
inline constexpr ScheduleLabel ExtractRender = ScheduleLabel::ExtractRender;
inline constexpr ScheduleLabel Render = ScheduleLabel::Render;
inline constexpr ScheduleLabel PostRender = ScheduleLabel::PostRender;
inline constexpr ScheduleLabel Last = ScheduleLabel::Last;
inline constexpr ScheduleLabel FixedUpdate = ScheduleLabel::FixedUpdate;

template <typename H> H AbslHashValue(H h, ScheduleLabel l) {
  return H::combine(std::move(h), static_cast<i32>(l));
}

} // namespace ecs

template <> struct std::hash<ecs::ScheduleLabel> {
  usize operator()(ecs::ScheduleLabel l) const noexcept {
    return std::hash<i32>{}(static_cast<i32>(l));
  }
};

namespace ecs {

inline constexpr const char *schedule_label_name(ScheduleLabel label) {
  switch (label) {
  case ScheduleLabel::PreStartup:
    return "PreStartup";
  case ScheduleLabel::Startup:
    return "Startup";
  case ScheduleLabel::PostStartup:
    return "PostStartup";
  case ScheduleLabel::First:
    return "First";
  case ScheduleLabel::PreUpdate:
    return "PreUpdate";
  case ScheduleLabel::Update:
    return "Update";
  case ScheduleLabel::PostUpdate:
    return "PostUpdate";
  case ScheduleLabel::ExtractRender:
    return "ExtractRender";
  case ScheduleLabel::Render:
    return "Render";
  case ScheduleLabel::PostRender:
    return "PostRender";
  case ScheduleLabel::Last:
    return "Last";
  case ScheduleLabel::FixedUpdate:
    return "FixedUpdate";
  }
  return "Unknown";
}

} // namespace ecs
