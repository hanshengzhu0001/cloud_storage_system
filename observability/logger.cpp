#include "logger.hpp"

#include <iomanip>
#include <sstream>

namespace banking {
namespace observability {

Logger& Logger::getInstance() {
  static Logger instance;
  return instance;
}

Logger::Logger() : min_level_(LogLevel::INFO), output_stream_(&std::cout) {}

void Logger::setLogLevel(LogLevel level) {
  min_level_ = level;
}

LogLevel Logger::getLogLevel() const {
  return min_level_;
}

void Logger::setOutputStream(std::ostream& stream) {
  std::lock_guard<std::mutex> lock(mutex_);
  output_stream_ = &stream;
}

void Logger::debug(const std::string& message, const std::string& component,
                   const std::string& correlation_id) {
  log(LogLevel::DEBUG, message, component, correlation_id);
}

void Logger::info(const std::string& message, const std::string& component,
                  const std::string& correlation_id) {
  log(LogLevel::INFO, message, component, correlation_id);
}

void Logger::warn(const std::string& message, const std::string& component,
                  const std::string& correlation_id) {
  log(LogLevel::WARN, message, component, correlation_id);
}

void Logger::error(const std::string& message, const std::string& component,
                   const std::string& correlation_id) {
  log(LogLevel::ERROR, message, component, correlation_id);
}

void Logger::fatal(const std::string& message, const std::string& component,
                   const std::string& correlation_id) {
  log(LogLevel::FATAL, message, component, correlation_id);
}

Logger::LogBuilder::LogBuilder(LogLevel level, const std::string& message,
                               const std::string& component,
                               const std::string& correlation_id)
    : level_(level), message_(message), component_(component),
      correlation_id_(correlation_id) {}

Logger::LogBuilder::~LogBuilder() {
  Logger::getInstance().log(level_, message_, component_, correlation_id_, fields_);
}

Logger::LogBuilder& Logger::LogBuilder::field(const std::string& key, const std::string& value) {
  fields_[key] = "\"" + value + "\"";
  return *this;
}

Logger::LogBuilder& Logger::LogBuilder::field(const std::string& key, int value) {
  fields_[key] = std::to_string(value);
  return *this;
}

Logger::LogBuilder& Logger::LogBuilder::field(const std::string& key, double value) {
  std::stringstream ss;
  ss << std::fixed << std::setprecision(6) << value;
  fields_[key] = ss.str();
  return *this;
}

Logger::LogBuilder& Logger::LogBuilder::field(const std::string& key, bool value) {
  fields_[key] = value ? "true" : "false";
  return *this;
}

void Logger::log(LogLevel level, const std::string& message,
                 const std::string& component, const std::string& correlation_id,
                 const std::unordered_map<std::string, std::string>& fields) {
  if (level < min_level_) return;

  std::lock_guard<std::mutex> lock(mutex_);

  // Build JSON log entry
  std::stringstream ss;
  ss << "{";
  ss << "\"timestamp\":\"" << getCurrentTimestamp() << "\",";
  ss << "\"level\":\"" << levelToString(level) << "\",";
  ss << "\"thread\":\"" << getThreadId() << "\",";
  ss << "\"message\":\"" << message << "\"";

  if (!component.empty()) {
    ss << ",\"component\":\"" << component << "\"";
  }

  if (!correlation_id.empty()) {
    ss << ",\"correlation_id\":\"" << correlation_id << "\"";
  }

  for (const auto& [key, value] : fields) {
    ss << ",\"" << key << "\":" << value;
  }

  ss << "}\n";

  *output_stream_ << ss.str();
  output_stream_->flush();
}

std::string Logger::levelToString(LogLevel level) const {
  switch (level) {
    case LogLevel::DEBUG: return "DEBUG";
    case LogLevel::INFO: return "INFO";
    case LogLevel::WARN: return "WARN";
    case LogLevel::ERROR: return "ERROR";
    case LogLevel::FATAL: return "FATAL";
    default: return "UNKNOWN";
  }
}

std::string Logger::getCurrentTimestamp() const {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(
      now.time_since_epoch()) % 1000000;

  std::stringstream ss;
  ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S")
     << "." << std::setfill('0') << std::setw(6) << microseconds.count() << "Z";
  return ss.str();
}

std::string Logger::getThreadId() const {
  std::stringstream ss;
  ss << std::this_thread::get_id();
  return ss.str();
}

}  // namespace observability
}  // namespace banking
