#ifndef BANKING_SYSTEM_PERSISTENT_HPP_
#define BANKING_SYSTEM_PERSISTENT_HPP_

#include "banking_system.hpp"
#include "database/banking_persistence.hpp"

#include <memory>

namespace banking {

/**
 * Persistent banking system that combines in-memory operations with database persistence.
 * Uses write-through caching strategy: operations are performed in memory first,
 * then persisted to database for durability.
 */
class BankingSystemPersistent : public BankingSystem {
 public:
  /**
   * Configuration for the persistent banking system.
   */
  struct Config {
    std::string db_host = "localhost";
    int db_port = 5432;
    std::string db_name = "banking_system";
    std::string db_username = "banking_user";
    std::string db_password = "";
    bool enable_fraud_detection = true;
    bool enable_audit_logging = true;
  };

  BankingSystemPersistent(const Config& config);
  ~BankingSystemPersistent() override;

  // Non-copyable
  BankingSystemPersistent(const BankingSystemPersistent&) = delete;
  BankingSystemPersistent& operator=(const BankingSystemPersistent&) = delete;

  /**
   * Initialize the database schema and load existing data.
   */
  bool initialize();

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
   * Helper methods for persistence operations.
   */
  bool persistTransaction(const std::string& transaction_type,
                         const std::string& account_id,
                         int amount, int balance_before, int balance_after,
                         int timestamp, const std::string& reference_id = "",
                         const std::string& description = "");

  bool loadFromDatabase();
  bool syncInMemoryWithDatabase();

  Config config_;
  std::unique_ptr<BankingSystem> memory_system_;
  std::shared_ptr<database::PostgresConnection> db_connection_;
  std::unique_ptr<database::BankingPersistence> persistence_;

  // Cache for frequently accessed data
  std::unordered_map<std::string, int> account_creation_cache_;
  mutable std::shared_mutex cache_mutex_;
};

}  // namespace banking

#endif  // BANKING_SYSTEM_PERSISTENT_HPP_
