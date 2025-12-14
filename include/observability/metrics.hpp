#ifndef METRICS_HPP_
#define METRICS_HPP_

#include <atomic>
#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

namespace banking {
namespace observability {

/**
 * Simple metrics collection system for monitoring system performance.
 * Supports counters, gauges, and histograms with Prometheus-compatible output.
 */
class MetricsCollector {
 public:
  MetricsCollector();
  ~MetricsCollector() = default;

  // Counter: monotonically increasing value
  void incrementCounter(const std::string& name, double value = 1.0);

  // Gauge: value that can go up and down
  void setGauge(const std::string& name, double value);
  void incrementGauge(const std::string& name, double value = 1.0);
  void decrementGauge(const std::string& name, double value = 1.0);

  // Histogram: distribution of values
  void observeHistogram(const std::string& name, double value);

  // Timer helpers
  class Timer {
   public:
    Timer(MetricsCollector& collector, const std::string& name);
    ~Timer();

   private:
    MetricsCollector& collector_;
    std::string name_;
    std::chrono::steady_clock::time_point start_;
  };

  // Export metrics in Prometheus format
  std::string exportMetrics() const;

  // Reset all metrics
  void reset();

 private:
  struct Counter {
    double value{0.0};
    std::string help;
  };

  struct Gauge {
    double value{0.0};
    std::string help;
  };

  struct HistogramBucket {
    double upper_bound;
    size_t count{0};
  };

  struct Histogram {
    std::vector<HistogramBucket> buckets;
    size_t count{0};
    double sum{0.0};
    std::string help;
  };

  mutable std::mutex mutex_;
  std::unordered_map<std::string, Counter> counters_;
  std::unordered_map<std::string, Gauge> gauges_;
  std::unordered_map<std::string, Histogram> histograms_;

  // Default histogram buckets (in seconds)
  static std::vector<double> defaultBuckets();
};

// Global metrics instance
MetricsCollector& getGlobalMetrics();

}  // namespace observability
}  // namespace banking

#endif  // METRICS_HPP_
