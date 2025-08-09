#ifndef BANKING_SYSTEM_IMPL_HPP_
#define BANKING_SYSTEM_IMPL_HPP_

#include "banking_system.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <map>

// Level 1 implementation notes:
// - Stores everything in memory using a simple map of account_id -> balance.
// - Timestamps are accepted for API compatibility but are not used at this level.
// - No persistence, concurrency, or overdraft features are implemented.

/**
 * Minimal in-memory implementation of the BankingSystem interface for Level 1.
 *
 * Responsibilities:
 * - Create new accounts
 * - Deposit money into existing accounts
 * - Transfer money between two distinct existing accounts
 */
class BankingSystemImpl : public BankingSystem {
 public:
  BankingSystemImpl() = default;

  /**
   * Creates a new account with zero balance.
   */
  bool CreateAccount(int timestamp, const std::string& account_id) override;

  /**
   * Deposits `amount` into the specified account and returns the new balance.
   */
  std::optional<int> Deposit(int timestamp, const std::string& account_id, int amount) override;

  /**
   * Transfers `amount` from `source_account_id` to `target_account_id`.
   */
  std::optional<int> Transfer(int timestamp, const std::string& source_account_id, const std::string& target_account_id, int amount) override;

  /**
   * Returns formatted identifiers of top n accounts by total outgoing amount.
   */
  std::vector<std::string> TopSpenders(int timestamp, int n) override;

  /** Schedule/Cancel payments (Level 3). */
  std::optional<std::string> SchedulePayment(int timestamp, const std::string& account_id, int amount, int delay) override;
  bool CancelPayment(int timestamp, const std::string& account_id, const std::string& payment_id) override;

  /** Merge accounts and historical balance query (Level 4). */
  bool MergeAccounts(int timestamp, const std::string& account_id_1, const std::string& account_id_2) override;
  std::optional<int> GetBalance(int timestamp, const std::string& account_id, int time_at) override;

 private:
  // Process all scheduled payments due at or before `timestamp`.
  void processDuePayments(int timestamp);

  // Resolve the owner id for `account_id` at a specific time point.
  std::string rootAtTime(const std::string& account_id, int time_at) const;

  // Current balance per account identifier (smallest currency unit, e.g. cents).
  std::unordered_map<std::string, int> accountBalances_;
  // Total outgoing (successful transfers and, in future levels, payments) per account.
  std::unordered_map<std::string, int> accountOutgoing_;

  // Global payment ordinal counter for generating unique ids.
  int nextPaymentOrdinal_ = 1;

  // Payment record stored for tracking and cancellation.
  struct PaymentInfo {
    std::string accountId;
    int amount;
    int dueTimestamp;
    bool canceled = false;
    bool processed = false;  // set when attempted (success or skipped)
    int creationOrder;  // strictly increasing
  };

  // Map payment id -> info (for cancel and lookup validations)
  std::unordered_map<std::string, PaymentInfo> paymentById_;

  // For processing by timestamp in order, and by creation order for same timestamp (global creation order).
  // map keyed by dueTimestamp -> vector of payment_ids to preserve insertion order for same timestamp.
  std::map<int, std::vector<std::string>> dueTimeToPaymentIds_;

  // Track balance deltas per account for historical queries.
  // Each entry: (timestamp, delta). Sum of deltas up to time_at yields balance at that time for that account.
  std::unordered_map<std::string, std::vector<std::pair<int, int>>> balanceEvents_;

  // Direct merge edges: child -> (parent, mergeTimestamp)
  std::unordered_map<std::string, std::pair<std::string, int>> mergedInto_;

  // Account creation time for existence checks in GetBalance
  std::unordered_map<std::string, int> accountCreationTime_;

  // Level 4: support multiple lifetimes of the same account id (re-creation after merge)
  std::unordered_map<std::string, std::vector<int>> creationTimes_;
  std::unordered_map<std::string, std::vector<int>> deletionTimes_;  // merge timestamps per id
};

#endif  // BANKING_SYSTEM_IMPL_HPP_
