#ifndef BANKING_SYSTEM_THREAD_SAFE_HPP_
#define BANKING_SYSTEM_THREAD_SAFE_HPP_

#include "banking_system.hpp"

#include <memory>
#include <shared_mutex>
#include <unordered_map>
#include <mutex>

namespace banking {

/**
 * Thread-safe wrapper for BankingSystem with fine-grained locking.
 * Uses account-level locking to allow concurrent operations on different accounts.
 */
class BankingSystemThreadSafe : public BankingSystem {
 public:
  explicit BankingSystemThreadSafe(std::unique_ptr<BankingSystem> impl);
  ~BankingSystemThreadSafe() override = default;

  // Non-copyable
  BankingSystemThreadSafe(const BankingSystemThreadSafe&) = delete;
  BankingSystemThreadSafe& operator=(const BankingSystemThreadSafe&) = delete;

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
  std::optional<int> Transfer(int timestamp, const std::string& source_account_id,
                             const std::string& target_account_id, int amount) override;

  /**
   * Returns formatted identifiers of top n accounts by total outgoing amount.
   */
  std::vector<std::string> TopSpenders(int timestamp, int n) override;

  /** Schedule/Cancel payments (Level 3). */
  std::optional<std::string> SchedulePayment(int timestamp, const std::string& account_id,
                                           int amount, int delay) override;
  bool CancelPayment(int timestamp, const std::string& account_id,
                    const std::string& payment_id) override;

  /** Merge accounts and historical balance query (Level 4). */
  bool MergeAccounts(int timestamp, const std::string& account_id_1,
                    const std::string& account_id_2) override;
  std::optional<int> GetBalance(int timestamp, const std::string& account_id,
                               int time_at) override;

 private:
  /**
   * Get or create a mutex for the given account.
   */
  std::shared_mutex& getAccountMutex(const std::string& account_id);

  /**
   * Acquire locks for multiple accounts in consistent order to prevent deadlocks.
   */
  void acquireAccountLocks(const std::string& account1, const std::string& account2,
                          std::unique_lock<std::shared_mutex>& lock1,
                          std::unique_lock<std::shared_mutex>& lock2);

  std::unique_ptr<BankingSystem> impl_;
  std::unordered_map<std::string, std::unique_ptr<std::shared_mutex>> account_mutexes_;
  mutable std::shared_mutex global_mutex_;  // For global operations like TopSpenders
};

}  // namespace banking

#endif  // BANKING_SYSTEM_THREAD_SAFE_HPP_
