#ifndef FRAUD_DETECTION_AGENT_HPP_
#define FRAUD_DETECTION_AGENT_HPP_

#include "../network/protocol.hpp"
#include "../concurrent/lockfree_queue.hpp"

#include <atomic>
#include <memory>
#include <thread>
#include <unordered_map>
#include <vector>
#include <deque>
#include <chrono>
#include <functional>

namespace banking {
namespace ai {

/**
 * Transaction data for fraud analysis.
 */
struct TransactionData {
  std::string account_id;
  std::string transaction_type;
  int amount;
  int timestamp;
  std::string source_ip;
  std::string location;
  std::unordered_map<std::string, std::string> metadata;

  TransactionData(const std::string& acc_id, const std::string& type, int amt,
                 int ts, const std::string& ip = "", const std::string& loc = "")
      : account_id(acc_id), transaction_type(type), amount(amt), timestamp(ts),
        source_ip(ip), location(loc) {}
};

/**
 * Fraud detection result.
 */
struct FraudResult {
  double risk_score;  // 0.0 to 1.0
  std::vector<std::string> risk_factors;
  std::string recommendation;  // "ALLOW", "BLOCK", "REVIEW"
  int confidence_level;  // 0-100

  bool isFraudulent() const { return risk_score > 0.7; }
  bool needsReview() const { return risk_score > 0.4 && risk_score <= 0.7; }
};

/**
 * AI-powered fraud detection agent.
 * Analyzes transaction patterns in real-time to detect fraudulent activity.
 */
class FraudDetectionAgent {
 public:
  using AlertCallback = std::function<void(const TransactionData&, const FraudResult&)>;

  FraudDetectionAgent(size_t analysis_window_seconds = 3600,  // 1 hour
                      size_t max_transactions_per_account = 1000);
  ~FraudDetectionAgent();

  // Non-copyable
  FraudDetectionAgent(const FraudDetectionAgent&) = delete;
  FraudDetectionAgent& operator=(const FraudDetectionAgent&) = delete;

  /**
   * Start the fraud detection agent.
   */
  bool start();

  /**
   * Stop the fraud detection agent.
   */
  void stop();

  /**
   * Analyze a transaction for fraud.
   */
  FraudResult analyzeTransaction(const TransactionData& transaction);

  /**
   * Submit transaction for asynchronous analysis.
   */
  void submitTransaction(const TransactionData& transaction);

  /**
   * Set callback for fraud alerts.
   */
  void setAlertCallback(AlertCallback callback);

  /**
   * Get current analysis statistics.
   */
  struct Stats {
    size_t transactions_analyzed;
    size_t fraud_alerts_generated;
    double average_risk_score;
    size_t analysis_queue_size;
  };
  Stats getStats() const;

  /**
   * Update fraud detection models (simulated learning).
   */
  void updateModels();

 private:
  void analysisWorker();
  FraudResult performAnalysis(const TransactionData& transaction);
  double calculateAmountAnomalyScore(const TransactionData& transaction);
  double calculateFrequencyAnomalyScore(const TransactionData& transaction);
  double calculateVelocityAnomalyScore(const TransactionData& transaction);
  double calculateLocationAnomalyScore(const TransactionData& transaction);

  // Transaction history per account
  struct AccountHistory {
    std::deque<TransactionData> recent_transactions;
    std::chrono::steady_clock::time_point last_update;
    double average_transaction_amount = 0.0;
    double transaction_frequency_per_hour = 0.0;
    std::unordered_map<std::string, int> location_counts;
    std::unordered_map<std::string, int> ip_counts;

    void addTransaction(const TransactionData& tx);
    void cleanupOldTransactions(int current_timestamp);
  };

  size_t analysis_window_seconds_;
  size_t max_transactions_per_account_;

  std::unordered_map<std::string, AccountHistory> account_histories_;
  mutable std::mutex histories_mutex_;

  concurrent::LockFreeQueue<TransactionData> analysis_queue_;
  std::unique_ptr<std::thread> analysis_thread_;
  std::atomic<bool> running_;

  AlertCallback alert_callback_;

  // Statistics
  std::atomic<size_t> transactions_analyzed_;
  std::atomic<size_t> fraud_alerts_generated_;
  double total_risk_score_;
  mutable std::mutex stats_mutex_;

  // Fraud detection thresholds (configurable)
  double amount_anomaly_threshold_ = 3.0;  // Standard deviations
  double frequency_anomaly_threshold_ = 5.0;  // Transactions per hour
  double velocity_threshold_ = 10000;  // Max amount per hour
  double location_diversity_threshold_ = 0.8;  // Max location entropy
};

}  // namespace ai
}  // namespace banking

#endif  // FRAUD_DETECTION_AGENT_HPP_
