#ifndef BANKING_SYSTEM_HPP_
#define BANKING_SYSTEM_HPP_

#include <optional>
#include <string>
#include <vector>

/**
 * Abstract base class for banking system operations.
 * This provides the interface that implementations must follow.
 */
class BankingSystem {
 public:
  virtual ~BankingSystem() = default;

  /**
   * Creates a new account with zero balance.
   */
  virtual bool CreateAccount(int timestamp, const std::string& account_id) = 0;

  /**
   * Deposits `amount` into the specified account and returns the new balance.
   */
  virtual std::optional<int> Deposit(int timestamp, const std::string& account_id, int amount) = 0;

  /**
   * Transfers `amount` from `source_account_id` to `target_account_id`.
   */
  virtual std::optional<int> Transfer(int timestamp, const std::string& source_account_id,
                                     const std::string& target_account_id, int amount) = 0;

  /**
   * Returns formatted identifiers of top n accounts by total outgoing amount.
   */
  virtual std::vector<std::string> TopSpenders(int timestamp, int n) = 0;

  /** Schedule/Cancel payments (Level 3). */
  virtual std::optional<std::string> SchedulePayment(int timestamp, const std::string& account_id,
                                                   int amount, int delay) = 0;
  virtual bool CancelPayment(int timestamp, const std::string& account_id,
                           const std::string& payment_id) = 0;

  /** Merge accounts and historical balance query (Level 4). */
  virtual bool MergeAccounts(int timestamp, const std::string& account_id_1,
                           const std::string& account_id_2) = 0;
  virtual std::optional<int> GetBalance(int timestamp, const std::string& account_id,
                                      int time_at) = 0;
};

#endif  // BANKING_SYSTEM_HPP_
