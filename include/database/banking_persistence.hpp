#ifndef BANKING_PERSISTENCE_HPP_
#define BANKING_PERSISTENCE_HPP_

#include "postgres_connection.hpp"
#include "../network/protocol.hpp"

#include <optional>
#include <vector>
#include <memory>

namespace banking {
namespace database {

/**
 * Transaction data structure for database operations.
 */
struct TransactionRecord {
  std::string id;
  std::string account_id;
  std::string transaction_type;
  int amount;
  int balance_before;
  int balance_after;
  int timestamp;
  std::string reference_id;
  std::string description;
  std::map<std::string, std::string> metadata;

  TransactionRecord() = default;
  TransactionRecord(const std::string& acc_id, const std::string& type, int amt,
                   int bal_before, int bal_after, int ts,
                   const std::string& ref_id = "", const std::string& desc = "")
      : account_id(acc_id), transaction_type(type), amount(amt),
        balance_before(bal_before), balance_after(bal_after), timestamp(ts),
        reference_id(ref_id), description(desc) {}
};

/**
 * Scheduled payment data structure.
 */
struct ScheduledPaymentRecord {
  std::string payment_id;
  std::string account_id;
  int amount;
  int due_timestamp;
  int created_at;
  bool is_canceled;
  bool is_processed;
  int processing_timestamp;
  int creation_order;

  ScheduledPaymentRecord() = default;
  ScheduledPaymentRecord(const std::string& pay_id, const std::string& acc_id,
                        int amt, int due_ts, int created_ts = 0,
                        bool canceled = false, bool processed = false)
      : payment_id(pay_id), account_id(acc_id), amount(amt), due_timestamp(due_ts),
        created_at(created_ts), is_canceled(canceled), is_processed(processed) {}
};

/**
 * Balance event for historical queries.
 */
struct BalanceEvent {
  int timestamp;
  int balance_delta;
  std::string event_type;

  BalanceEvent(int ts = 0, int delta = 0, const std::string& type = "")
      : timestamp(ts), balance_delta(delta), event_type(type) {}
};

/**
 * Banking persistence interface.
 * Provides database operations for the banking system.
 */
class BankingPersistence {
 public:
  BankingPersistence(std::shared_ptr<PostgresConnection> conn);
  virtual ~BankingPersistence() = default;

  // Non-copyable
  BankingPersistence(const BankingPersistence&) = delete;
  BankingPersistence& operator=(const BankingPersistence&) = delete;

  /**
   * Initialize database schema.
   */
  bool initializeSchema();

  // Account operations
  virtual bool createAccount(const std::string& account_id, int initial_balance = 0);
  virtual bool accountExists(const std::string& account_id);
  virtual std::optional<int> getAccountBalance(const std::string& account_id);
  virtual bool updateAccountBalance(const std::string& account_id, int new_balance);

  // Transaction operations
  virtual bool saveTransaction(const TransactionRecord& transaction);
  virtual std::vector<TransactionRecord> getAccountTransactions(const std::string& account_id,
                                                              int limit = 100,
                                                              int offset = 0);
  virtual std::optional<int> getAccountOutgoingTotal(const std::string& account_id);

  // Scheduled payment operations
  virtual bool saveScheduledPayment(const ScheduledPaymentRecord& payment);
  virtual std::optional<ScheduledPaymentRecord> getScheduledPayment(const std::string& payment_id);
  virtual bool updateScheduledPayment(const std::string& payment_id, bool is_processed,
                                    int processing_timestamp = 0);
  virtual std::vector<ScheduledPaymentRecord> getDuePayments(int current_timestamp);
  virtual bool cancelScheduledPayment(const std::string& payment_id);

  // Historical balance operations
  virtual bool saveBalanceEvent(const std::string& account_id, const BalanceEvent& event);
  virtual std::vector<BalanceEvent> getBalanceEvents(const std::string& account_id,
                                                   int start_time = 0, int end_time = INT_MAX);
  virtual std::optional<int> getBalanceAtTime(const std::string& account_id, int time_at);

  // Account merge operations
  virtual bool saveAccountMerge(const std::string& child_account_id,
                               const std::string& parent_account_id,
                               int merge_timestamp, int balance_transferred);
  virtual std::optional<std::pair<std::string, int>> getAccountMergeInfo(const std::string& account_id);
  virtual std::string resolveAccountAtTime(const std::string& account_id, int time_at);

  // Analytics operations
  virtual std::vector<std::pair<std::string, int>> getTopSpenders(int limit = 10);
  virtual std::map<std::string, int> getAccountCreationTimes();

  // Fraud detection operations
  virtual bool saveFraudAlert(const std::string& account_id, const std::string& transaction_id,
                             double risk_score, const std::vector<std::string>& risk_factors,
                             const std::string& recommendation, int confidence_level);

  // System operations
  virtual bool logSystemEvent(const std::string& event_type, const std::string& severity,
                             const std::string& message, const std::string& component = "",
                             const std::string& correlation_id = "");

 protected:
  std::shared_ptr<PostgresConnection> conn_;

 private:
  /**
   * Helper to execute schema creation from file.
   */
  bool executeSchemaFile(const std::string& schema_path);
};

}  // namespace database
}  // namespace banking

#endif  // BANKING_PERSISTENCE_HPP_
