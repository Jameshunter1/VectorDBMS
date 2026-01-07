#pragma once

#include <string_view>

namespace core_engine {

// A tiny logging facade.
//
// Why not pick a big logging library now?
// - For a multi-year engine, logging needs tend to evolve.
// - This facade keeps core code decoupled from your eventual choice
//   (spdlog, ETW, custom ring-buffer, syslog, etc.).
//
// Current policy: log to stderr (see logger.cpp). Replace later as needed.

enum class LogLevel {
  kDebug,
  kInfo,
  kWarn,
  kError,
};

void Log(LogLevel level, std::string_view message);
void SetMinLogLevel(LogLevel level);
LogLevel GetMinLogLevel();

} // namespace core_engine
