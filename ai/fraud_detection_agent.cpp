#include "fraud_detection_agent.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>
#include <random>

namespace banking {
namespace ai {

FraudDetectionAgent::FraudDetectionAgent(size_t analysis_window_seconds,
                                       size_t max_transactions_per_account)
    : analysis_window_seconds_(analysis_window_seconds),
      max_transactions_per_account_(max_transactions_per_account),
      running_(false),
      transactions_analyzed_(0),
      fraud_alerts_generated_(0),
      total_risk_score_(0.0) {
}

FraudDetectionAgent::~FraudDetectionAgent() {
  stop();
}

bool FraudDetectionAgent::start() {
  if (running_) return true;

  running_ = true;
  analysis_thread_ = std::make_unique<std::thread>(&FraudDetectionAgent::analysisWorker, this);

  std::cout << "Fraud detection agent started" << std::endl;
  return true;
}

void FraudDetectionAgent::stop() {
  if (!running_) return;

  running_ = false;

  // Submit empty transaction to wake up worker
  submitTransaction(TransactionData("", "", 0, 0));

  if (analysis_thread_ && analysis_thread_->joinable()) {
    analysis_thread_->join();
  }

  std::cout << "Fraud detection agent stopped" << std::endl;
}

FraudResult FraudDetectionAgent::analyzeTransaction(const TransactionData& transaction) {
  return performAnalysis(transaction);
}

void FraudDetectionAgent::submitTransaction(const TransactionData& transaction) {
  analysis_queue_.enqueue(transaction);
}

void FraudDetectionAgent::setAlertCallback(AlertCallback callback) {
  alert_callback_ = std::move(callback);
}

FraudDetectionAgent::Stats FraudDetectionAgent::getStats() const {
  Stats stats;
  stats.transactions_analyzed = transactions_analyzed_.load();
  stats.fraud_alerts_generated = fraud_alerts_generated_.load();
  stats.analysis_queue_size = analysis_queue_.size();

  size_t total_tx = transactions_analyzed_.load();
  if (total_tx > 0) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats.average_risk_score = total_risk_score_ / total_tx;
  } else {
    stats.average_risk_score = 0.0;
  }

  return stats;
}

void FraudDetectionAgent::updateModels() {
  // Simulate model updates (in real implementation, this would retrain ML models)
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<> dis(-0.1, 0.1);

  amount_anomaly_threshold_ += dis(gen);
  frequency_anomaly_threshold_ += dis(gen);
  velocity_threshold_ += dis(gen) * 1000;

  // Keep thresholds within reasonable bounds
  amount_anomaly_threshold_ = std::max(1.5, std::min(5.0, amount_anomaly_threshold_));
  frequency_anomaly_threshold_ = std::max(2.0, std::min(10.0, frequency_anomaly_threshold_));
  velocity_threshold_ = std::max(5000.0, std::min(50000.0, velocity_threshold_));

  std::cout << "Fraud detection models updated" << std::endl;
}

void FraudDetectionAgent::analysisWorker() {
  while (running_) {
    auto transaction_opt = analysis_queue_.dequeue();
    if (!transaction_opt.has_value()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }

    const TransactionData& transaction = *transaction_opt;

    // Empty transaction signals shutdown
    if (transaction.account_id.empty()) {
      break;
    }

    FraudResult result = performAnalysis(transaction);
    transactions_analyzed_.fetch_add(1);
    {
      std::lock_guard<std::mutex> lock(stats_mutex_);
      total_risk_score_ += result.risk_score;
    }

    // Trigger alert if fraudulent or needs review
    if ((result.isFraudulent() || result.needsReview()) && alert_callback_) {
      alert_callback_(transaction, result);
      if (result.isFraudulent()) {
        fraud_alerts_generated_.fetch_add(1);
      }
    }
  }
}

FraudResult FraudDetectionAgent::performAnalysis(const TransactionData& transaction) {
  FraudResult result;
  result.risk_factors.clear();

  // Calculate different risk scores
  double amount_score = calculateAmountAnomalyScore(transaction);
  double frequency_score = calculateFrequencyAnomalyScore(transaction);
  double velocity_score = calculateVelocityAnomalyScore(transaction);
  double location_score = calculateLocationAnomalyScore(transaction);

  // Combine scores with weights
  result.risk_score = (amount_score * 0.4) + (frequency_score * 0.3) +
                     (velocity_score * 0.2) + (location_score * 0.1);

  // Clamp to [0, 1]
  result.risk_score = std::max(0.0, std::min(1.0, result.risk_score));

  // Determine recommendation
  if (result.risk_score > 0.8) {
    result.recommendation = "BLOCK";
    result.confidence_level = 95;
  } else if (result.risk_score > 0.6) {
    result.recommendation = "REVIEW";
    result.confidence_level = 85;
  } else if (result.risk_score > 0.3) {
    result.recommendation = "MONITOR";
    result.confidence_level = 70;
  } else {
    result.recommendation = "ALLOW";
    result.confidence_level = 90;
  }

  // Add risk factors
  if (amount_score > 0.5) {
    result.risk_factors.push_back("Unusual transaction amount");
  }
  if (frequency_score > 0.5) {
    result.risk_factors.push_back("High transaction frequency");
  }
  if (velocity_score > 0.5) {
    result.risk_factors.push_back("High velocity spending");
  }
  if (location_score > 0.5) {
    result.risk_factors.push_back("Unusual location pattern");
  }

  return result;
}

double FraudDetectionAgent::calculateAmountAnomalyScore(const TransactionData& transaction) {
  std::lock_guard<std::mutex> lock(histories_mutex_);
  auto it = account_histories_.find(transaction.account_id);
  if (it == account_histories_.end() || it->second.average_transaction_amount == 0.0) {
    return 0.0;  // No history available
  }

  const AccountHistory& history = it->second;
  double mean = history.average_transaction_amount;
  double amount = static_cast<double>(transaction.amount);

  // Simple z-score calculation (assuming normal distribution)
  double std_dev = mean * 0.5;  // Rough estimate
  double z_score = std::abs(amount - mean) / std_dev;

  // Convert to risk score [0, 1]
  return std::min(1.0, z_score / amount_anomaly_threshold_);
}

double FraudDetectionAgent::calculateFrequencyAnomalyScore(const TransactionData& transaction) {
  std::lock_guard<std::mutex> lock(histories_mutex_);
  auto it = account_histories_.find(transaction.account_id);
  if (it == account_histories_.end()) {
    return 0.0;
  }

  const AccountHistory& history = it->second;
  double current_freq = history.transaction_frequency_per_hour;

  // Compare to threshold
  if (current_freq > frequency_anomaly_threshold_) {
    return std::min(1.0, current_freq / (frequency_anomaly_threshold_ * 2.0));
  }

  return 0.0;
}

double FraudDetectionAgent::calculateVelocityAnomalyScore(const TransactionData& transaction) {
  std::lock_guard<std::mutex> lock(histories_mutex_);
  auto it = account_histories_.find(transaction.account_id);
  if (it == account_histories_.end() || it->second.recent_transactions.empty()) {
    return 0.0;
  }

  const AccountHistory& history = it->second;

  // Calculate total amount in last hour
  auto one_hour_ago = transaction.timestamp - 3600;
  int total_amount_last_hour = 0;

  for (const auto& tx : history.recent_transactions) {
    if (tx.timestamp >= one_hour_ago) {
      total_amount_last_hour += tx.amount;
    }
  }

  total_amount_last_hour += transaction.amount;

  if (total_amount_last_hour > velocity_threshold_) {
    return std::min(1.0, static_cast<double>(total_amount_last_hour) / (velocity_threshold_ * 2.0));
  }

  return 0.0;
}

double FraudDetectionAgent::calculateLocationAnomalyScore(const TransactionData& transaction) {
  if (transaction.location.empty()) {
    return 0.0;  // No location data
  }

  std::lock_guard<std::mutex> lock(histories_mutex_);
  auto it = account_histories_.find(transaction.account_id);
  if (it == account_histories_.end() || it->second.location_counts.empty()) {
    return 0.0;
  }

  const auto& location_counts = it->second.location_counts;
  int total_locations = 0;
  int current_location_count = 0;

  for (const auto& pair : location_counts) {
    total_locations += pair.second;
    if (pair.first == transaction.location) {
      current_location_count = pair.second;
    }
  }

  if (total_locations == 0) return 0.0;

  double location_ratio = static_cast<double>(current_location_count) / total_locations;

  // Lower ratio means more unusual location
  return std::max(0.0, 1.0 - location_ratio);
}

void FraudDetectionAgent::AccountHistory::addTransaction(const TransactionData& tx) {
  recent_transactions.push_back(tx);

  // Maintain max size
  while (recent_transactions.size() > 1000) {  // Configurable
    recent_transactions.pop_front();
  }

  // Update statistics
  if (!recent_transactions.empty()) {
    double total_amount = 0.0;
    for (const auto& t : recent_transactions) {
      total_amount += t.amount;
    }
    average_transaction_amount = total_amount / recent_transactions.size();

    // Update location and IP counts
    if (!tx.location.empty()) {
      location_counts[tx.location]++;
    }
    if (!tx.source_ip.empty()) {
      ip_counts[tx.source_ip]++;
    }

    // Calculate frequency (transactions per hour)
    if (recent_transactions.size() >= 2) {
      auto time_span = recent_transactions.back().timestamp - recent_transactions.front().timestamp;
      if (time_span > 0) {
        transaction_frequency_per_hour = (recent_transactions.size() * 3600.0) / time_span;
      }
    }
  }

  last_update = std::chrono::steady_clock::now();
}

void FraudDetectionAgent::AccountHistory::cleanupOldTransactions(int current_timestamp) {
  int cutoff_time = current_timestamp - 3600;  // 1 hour window

  while (!recent_transactions.empty() && recent_transactions.front().timestamp < cutoff_time) {
    recent_transactions.pop_front();
  }
}

}  // namespace ai
}  // namespace banking
