#pragma once
#include "rl.hpp"
#include "rydz_ecs/app.hpp"
#include <cstdlib>
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <string>

template <typename... Args>
auto trace(fmt::format_string<Args...> fmt, Args &&...args) -> void {
  spdlog::trace(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
auto debug(fmt::format_string<Args...> fmt, Args &&...args) -> void {
  spdlog::debug(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
auto info(fmt::format_string<Args...> fmt, Args &&...args) -> void {
  spdlog::info(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
auto warn(fmt::format_string<Args...> fmt, Args &&...args) -> void {
  spdlog::warn(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
auto error(fmt::format_string<Args...> fmt, Args &&...args) -> void {
  spdlog::error(fmt, std::forward<Args>(args)...);
}

// Runtime format string support
template <typename... Args>
auto trace(fmt::runtime_format_string<Args...> fmt, Args &&...args) -> void {
  spdlog::trace(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
auto debug(fmt::runtime_format_string<Args...> fmt, Args &&...args) -> void {
  spdlog::debug(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
auto info(fmt::runtime_format_string<Args...> fmt, Args &&...args) -> void {
  spdlog::info(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
auto warn(fmt::runtime_format_string<Args...> fmt, Args &&...args) -> void {
  spdlog::warn(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
auto error(fmt::runtime_format_string<Args...> fmt, Args &&...args) -> void {
  spdlog::error(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
auto log(spdlog::level::level_enum level,
         fmt::runtime_format_string<Args...> fmt, Args &&...args) -> void {
  spdlog::log(level, fmt, std::forward<Args>(args)...);
}

inline auto init_logging() -> void {
  spdlog::set_pattern("%^[%T] [%l] %v%$");

  const char *env_level = std::getenv("RYDZ_LOG");

  spdlog::level::level_enum level = spdlog::level::info;

  if (env_level != nullptr) {
    std::string s(env_level);
    if (s == "trace") {
      level = spdlog::level::trace;
      rl::SetTraceLogLevel(LOG_TRACE);
    } else if (s == "debug") {
      level = spdlog::level::debug;
      rl::SetTraceLogLevel(LOG_DEBUG);
    } else if (s == "warn") {
      level = spdlog::level::warn;
      rl::SetTraceLogLevel(LOG_WARNING);
    } else if (s == "error") {
      level = spdlog::level::err;
      rl::SetTraceLogLevel(LOG_ERROR);
    } else if (s == "off") {
      level = spdlog::level::off;
      rl::SetTraceLogLevel(LOG_NONE);
    }
  }

  spdlog::set_level(level);

  spdlog::info("Logging level set to: {}",
               spdlog::level::to_string_view(level));
}

struct LogPlugin : public ecs::IPlugin {
  auto build(ecs::App &) -> void override { init_logging(); }
};

inline auto log_plugin(ecs::App &) -> void { init_logging(); }
