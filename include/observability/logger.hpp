#ifndef LOGGER_HPP_
#define LOGGER_HPP_

#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

namespace banking {
namespace observability {

/**
 * Log levels for structured logging.
 */
enum class LogLevel {
  DEBUG,
  INFO,
  WARN,
  ERROR,
  FATAL
};

/**
 * Structured logger with JSON output and configurable log levels.
 * Thread-safe and supports correlation IDs for request tracing.
 */
class Logger {
 public:
  static Logger& getInstance();

  // Non-copyable
  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;

  // Set minimum log level
  void setLogLevel(LogLevel level);
  LogLevel getLogLevel() const;

  // Set output stream (default: std::cout)
  void setOutputStream(std::ostream& stream);

  // Logging methods
  void debug(const std::string& message,
             const std::string& component = "",
             const std::string& correlation_id = "");

  void info(const std::string& message,
            const std::string& component = "",
            const std::string& correlation_id = "");

  void warn(const std::string& message,
            const std::string& component = "",
            const std::string& correlation_id = "");

  void error(const std::string& message,
             const std::string& component = "",
             const std::string& correlation_id = "");

  void fatal(const std::string& message,
             const std::string& component = "",
             const std::string& correlation_id = "");

  // Structured logging with key-value pairs
  class LogBuilder {
   public:
    LogBuilder(LogLevel level, const std::string& message,
               const std::string& component = "",
               const std::string& correlation_id = "");

    ~LogBuilder();

    LogBuilder& field(const std::string& key, const std::string& value);
    LogBuilder& field(const std::string& key, int value);
    LogBuilder& field(const std::string& key, double value);
    LogBuilder& field(const std::string& key, bool value);

   private:
    LogLevel level_;
    std::string message_;
    std::string component_;
    std::string correlation_id_;
    std::unordered_map<std::string, std::string> fields_;
  };

 private:
  Logger();
  ~Logger() = default;

  void log(LogLevel level, const std::string& message,
           const std::string& component,
           const std::string& correlation_id,
           const std::unordered_map<std::string, std::string>& fields = {});

  std::string levelToString(LogLevel level) const;
  std::string getCurrentTimestamp() const;
  std::string getThreadId() const;

  LogLevel min_level_;
  std::ostream* output_stream_;
  mutable std::mutex mutex_;
};

// Convenience macros for logging
#define LOG_DEBUG(msg) banking::observability::Logger::getInstance().debug(msg, __func__)
#define LOG_INFO(msg) banking::observability::Logger::getInstance().info(msg, __func__)
#define LOG_WARN(msg) banking::observability::Logger::getInstance().warn(msg, __func__)
#define LOG_ERROR(msg) banking::observability::Logger::getInstance().error(msg, __func__)
#define LOG_FATAL(msg) banking::observability::Logger::getInstance().fatal(msg, __func__)

// Structured logging helper
#define LOG_BUILDER(level, msg) \
  banking::observability::Logger::LogBuilder(level, msg, __func__)

}  // namespace observability
}  // namespace banking

#endif  // LOGGER_HPP_
