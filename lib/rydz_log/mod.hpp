#pragma once
#include "rl.hpp"
#include "rydz_ecs/app.hpp"
#include <cstdlib>
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <string>

#define trace(...) spdlog::trace(__VA_ARGS__)
#define debug(...) spdlog::debug(__VA_ARGS__)
#define info(...) spdlog::info(__VA_ARGS__)
#define warn(...) spdlog::warn(__VA_ARGS__)
#define error(...) spdlog::error(__VA_ARGS__)

template <typename... Args>
auto log(
  spdlog::level::level_enum level, fmt::runtime_format_string<Args...> fmt, Args&&... args
) -> void {
  spdlog::log(level, fmt, std::forward<Args>(args)...);
}

namespace detail {
inline auto init_logging() -> void {
  spdlog::set_pattern("%^[%T] [%l] [%s:%#] %v%$");

  char const* env_level = std::getenv("RYDZ_LOG");

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

  info("Logging level set to: {}", spdlog::level::to_string_view(level));
}
} // namespace detail

struct LogPlugin : public ecs::IPlugin {
  auto build(ecs::App&) -> void { detail::init_logging(); }
};
