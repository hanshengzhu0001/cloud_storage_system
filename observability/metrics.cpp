#include "metrics.hpp"

#include <algorithm>
#include <sstream>

namespace banking {
namespace observability {

MetricsCollector::MetricsCollector() = default;

void MetricsCollector::incrementCounter(const std::string& name, double value) {
  std::lock_guard<std::mutex> lock(mutex_);
  counters_[name].value += value;
}

void MetricsCollector::setGauge(const std::string& name, double value) {
  std::lock_guard<std::mutex> lock(mutex_);
  gauges_[name].value = value;
}

void MetricsCollector::incrementGauge(const std::string& name, double value) {
  std::lock_guard<std::mutex> lock(mutex_);
  gauges_[name].value += value;
}

void MetricsCollector::decrementGauge(const std::string& name, double value) {
  incrementGauge(name, -value);
}

void MetricsCollector::observeHistogram(const std::string& name, double value) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto& hist = histograms_[name];

  // Initialize buckets if not done
  if (hist.buckets.empty()) {
    auto buckets = defaultBuckets();
    for (double bound : buckets) {
      hist.buckets.push_back({bound, 0});
    }
    // Add +Inf bucket
    hist.buckets.push_back({std::numeric_limits<double>::infinity(), 0});
  }

  // Update histogram
  hist.count += 1;
  hist.sum += value;

  // Find appropriate bucket
  for (auto& bucket : hist.buckets) {
    if (value <= bucket.upper_bound) {
      bucket.count += 1;
    }
  }
}

MetricsCollector::Timer::Timer(MetricsCollector& collector, const std::string& name)
    : collector_(collector), name_(name), start_(std::chrono::steady_clock::now()) {
}

MetricsCollector::Timer::~Timer() {
  auto end = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start_);
  double seconds = duration.count() / 1000000.0;
  collector_.observeHistogram(name_, seconds);
}

std::string MetricsCollector::exportMetrics() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::stringstream ss;

  // Export counters
  for (const auto& [name, counter] : counters_) {
    ss << "# HELP " << name << " Counter metric\n";
    ss << "# TYPE " << name << " counter\n";
    ss << name << " " << counter.value << "\n";
  }

  // Export gauges
  for (const auto& [name, gauge] : gauges_) {
    ss << "# HELP " << name << " Gauge metric\n";
    ss << "# TYPE " << name << " gauge\n";
    ss << name << " " << gauge.value << "\n";
  }

  // Export histograms
  for (const auto& [name, hist] : histograms_) {
    ss << "# HELP " << name << " Histogram metric\n";
    ss << "# TYPE " << name << " histogram\n";

    size_t cumulative_count = 0;
    for (size_t i = 0; i < hist.buckets.size(); ++i) {
      const auto& bucket = hist.buckets[i];
      cumulative_count += bucket.count;

      if (bucket.upper_bound == std::numeric_limits<double>::infinity()) {
        ss << name << "_bucket{le=\"+Inf\"} " << cumulative_count << "\n";
      } else {
        ss << name << "_bucket{le=\"" << bucket.upper_bound << "\"} " << cumulative_count << "\n";
      }
    }

    ss << name << "_count " << hist.count << "\n";
    ss << name << "_sum " << hist.sum << "\n";
  }

  return ss.str();
}

void MetricsCollector::reset() {
  std::lock_guard<std::mutex> lock(mutex_);

  counters_.clear();
  gauges_.clear();
  histograms_.clear();
}

std::vector<double> MetricsCollector::defaultBuckets() {
  return {0.005, 0.01, 0.025, 0.05, 0.075, 0.1, 0.25, 0.5, 0.75, 1.0, 2.5, 5.0, 7.5, 10.0};
}

MetricsCollector& getGlobalMetrics() {
  static MetricsCollector instance;
  return instance;
}

}  // namespace observability
}  // namespace banking
