#ifndef GLBOIDS_LOGG_HPP
#define GLBOIDS_LOGG_HPP

#include <chrono>
#include <cstdio>
#include <string>

#include <fmt/chrono.h>
#include <fmt/core.h>

class Logger {
public:
  enum class Level { Trace = 0, Debug, Info, Warn, Error, Fatal, None };

  Logger(Level level, std::FILE *out) : level(level), out(out){};

  Level getLevel() { return level; }
  void setLevel(Level level) { this->level = level; }
  std::FILE *getOut() { return out; }
  void setOut(std::FILE *out) { this->out = out; }

  template <typename... T>
  void trace(fmt::format_string<T...> fmt, T &&...args) {
    if (level > Level::Trace)
      return;

    fmt::print(out, "{} TRACE ", getTime());
    fmt::print(out, fmt, args...);
    fmt::print(out, "\n");
    std::fflush(out);
  }

  template <typename... T>
  void debug(fmt::format_string<T...> fmt, T &&...args) {
    if (level > Level::Debug)
      return;

    fmt::print(out, "{} DEBUG ", getTime());
    fmt::print(out, fmt, args...);
    fmt::print(out, "\n");
    std::fflush(out);
  }

  template <typename... T>
  void info(fmt::format_string<T...> fmt, T &&...args) {
    if (level > Level::Info)
      return;

    fmt::print(out, "{} INFO  ", getTime());
    fmt::print(out, fmt, args...);
    fmt::print(out, "\n");
    std::fflush(out);
  }

  template <typename... T>
  void warn(fmt::format_string<T...> fmt, T &&...args) {
    if (level > Level::Warn)
      return;

    fmt::print(out, "{} WARN  ", getTime());
    fmt::print(out, fmt, args...);
    fmt::print(out, "\n");
    std::fflush(out);
  }

  template <typename... T>
  void error(fmt::format_string<T...> fmt, T &&...args) {
    if (level > Level::Error)
      return;

    fmt::print(out, "{} ERROR ", getTime());
    fmt::print(out, fmt, args...);
    fmt::print(out, "\n");
    std::fflush(out);
  }

  template <typename... T>
  void fatal(fmt::format_string<T...> fmt, T &&...args) {
    if (level > Level::Fatal)
      return;

    fmt::print(out, "{} FATAL ", getTime());
    fmt::print(out, fmt, args...);
    fmt::print(out, "\n");
    std::fflush(out);
  }

private:
  Level level;
  std::FILE *out;

  inline std::string getTime() {
    const auto now = std::chrono::system_clock::now();
    const auto duration = now.time_since_epoch();
    const auto sec = std::chrono::floor<std::chrono::seconds>(duration);
    const auto milli = std::chrono::floor<std::chrono::milliseconds>(duration);
    return fmt::format("{:%T}.{:0>3}", now, (milli - sec).count());
  }
};

#endif // GLBOIDS_LOG_HPP