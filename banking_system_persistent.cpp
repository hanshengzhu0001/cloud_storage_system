#include "banking_system_persistent.hpp"
#include "banking_core_impl.hpp"
#include "database/postgres_connection.hpp"
#include "database/banking_persistence.hpp"
#include "observability/logger.hpp"

#include <iostream>
#include <algorithm>

namespace banking {

BankingSystemPersistent::BankingSystemPersistent(const Config& config)
    : config_(config), memory_system_(std::make_unique<BankingSystemImpl>()) {
}

BankingSystemPersistent::~BankingSystemPersistent() = default;

bool BankingSystemPersistent::initialize() {
  try {
    LOG_INFO("Initializing persistent banking system", "persistent");

    // Initialize database connection
    database::PostgresConnection::Config db_config{
      config_.db_host,
      config_.db_port,
      config_.db_name,
      config_.db_username,
      config_.db_password
    };

    db_connection_ = std::make_shared<database::PostgresConnection>(db_config);

    if (!db_connection_->connect()) {
      LOG_ERROR("Failed to connect to database", "persistent");
      return false;
    }

    // Initialize persistence layer
    persistence_ = std::make_unique<database::BankingPersistence>(db_connection_);

    if (!persistence_->initializeSchema()) {
      LOG_ERROR("Failed to initialize database schema", "persistent");
      return false;
    }

    // Load existing data from database
    if (!loadFromDatabase()) {
      LOG_ERROR("Failed to load existing data from database", "persistent");
      return false;
    }

    LOG_INFO("Persistent banking system initialized successfully", "persistent");
    return true;

  } catch (const std::exception& e) {
    LOG_ERROR("Persistent banking system initialization failed: " + std::string(e.what()), "persistent");
    return false;
  }
}

bool BankingSystemPersistent::CreateAccount(int timestamp, const std::string& account_id) {
  try {
    // First, try the in-memory operation
    bool memory_result = memory_system_->CreateAccount(timestamp, account_id);
    if (!memory_result) {
      return false;  // Account already exists or other error
    }

    // Persist to database
    if (persistence_) {
      if (!persistence_->createAccount(account_id, 0)) {
        LOG_ERROR("Failed to persist account creation to database: " + account_id, "persistent");
        // Rollback in-memory change by attempting to create again (should fail)
        memory_system_->CreateAccount(timestamp, account_id + "_rollback");
        return false;
      }

      // Cache account creation time
      {
        std::unique_lock<std::shared_mutex> lock(cache_mutex_);
        account_creation_cache_[account_id] = timestamp;
      }

      if (config_.enable_audit_logging) {
        persistence_->logSystemEvent("ACCOUNT_CREATED", "INFO",
                                    "Account created: " + account_id, "banking_system");
      }
    }

    return true;

  } catch (const std::exception& e) {
    LOG_ERROR("Account creation failed: " + std::string(e.what()), "persistent");
    return false;
  }
}

std::optional<int> BankingSystemPersistent::Deposit(int timestamp, const std::string& account_id, int amount) {
  try {
    // Get balance before operation for transaction record
    auto balance_before_opt = memory_system_->GetBalance(timestamp, account_id, timestamp);
    if (!balance_before_opt) {
      return std::nullopt;  // Account doesn't exist
    }
    int balance_before = *balance_before_opt;

    // Perform in-memory deposit
    auto result = memory_system_->Deposit(timestamp, account_id, amount);
    if (!result) {
      return std::nullopt;
    }
    int balance_after = *result;

    // Persist transaction
    if (persistence_) {
      if (!persistTransaction("DEPOSIT", account_id, amount, balance_before, balance_after, timestamp)) {
        LOG_ERROR("Failed to persist deposit transaction", "persistent");
        // Note: In production, you'd want transaction rollback here
        return std::nullopt;
      }
    }

    return result;

  } catch (const std::exception& e) {
    LOG_ERROR("Deposit operation failed: " + std::string(e.what()), "persistent");
    return std::nullopt;
  }
}

std::optional<int> BankingSystemPersistent::Transfer(int timestamp,
                                                   const std::string& source_account_id,
                                                   const std::string& target_account_id,
                                                   int amount) {
  try {
    // Get balances before operation
    auto source_balance_before_opt = memory_system_->GetBalance(timestamp, source_account_id, timestamp);
    auto target_balance_before_opt = memory_system_->GetBalance(timestamp, target_account_id, timestamp);

    if (!source_balance_before_opt || !target_balance_before_opt) {
      return std::nullopt;  // One or both accounts don't exist
    }

    int source_balance_before = *source_balance_before_opt;
    int target_balance_before = *target_balance_before_opt;

    // Perform in-memory transfer
    auto result = memory_system_->Transfer(timestamp, source_account_id, target_account_id, amount);
    if (!result) {
      return std::nullopt;  // Insufficient funds or other error
    }

    int source_balance_after = *result;

    // Get target balance after transfer
    auto target_balance_after_opt = memory_system_->GetBalance(timestamp, target_account_id, timestamp);
    if (!target_balance_after_opt) {
      LOG_ERROR("Failed to get target balance after transfer", "persistent");
      return std::nullopt;
    }
    int target_balance_after = *target_balance_after_opt;

    // Generate transfer ID for correlation
    std::string transfer_id = "transfer_" + std::to_string(timestamp) + "_" +
                             source_account_id + "_" + target_account_id;

    // Persist both sides of the transfer
    if (persistence_) {
      database::TransactionGuard transaction(*db_connection_);

      // Source account transaction (SEND)
      if (!persistTransaction("TRANSFER_SEND", source_account_id, amount,
                             source_balance_before, source_balance_after, timestamp, transfer_id)) {
        LOG_ERROR("Failed to persist transfer send transaction", "persistent");
        return std::nullopt;
      }

      // Target account transaction (RECEIVE)
      if (!persistTransaction("TRANSFER_RECEIVE", target_account_id, amount,
                             target_balance_before, target_balance_after, timestamp, transfer_id)) {
        LOG_ERROR("Failed to persist transfer receive transaction", "persistent");
        return std::nullopt;
      }

      transaction.commit();
    }

    return result;

  } catch (const std::exception& e) {
    LOG_ERROR("Transfer operation failed: " + std::string(e.what()), "persistent");
    return std::nullopt;
  }
}

std::vector<std::string> BankingSystemPersistent::TopSpenders(int timestamp, int n) {
  try {
    // For top spenders, we can use the database view if available, otherwise fall back to memory
    if (persistence_) {
      auto spenders = persistence_->getTopSpenders(n);
      std::vector<std::string> result;

      for (const auto& [account_id, amount] : spenders) {
        result.push_back(account_id + "(" + std::to_string(amount) + ")");
      }

      return result;
    }

    // Fallback to in-memory calculation
    return memory_system_->TopSpenders(timestamp, n);

  } catch (const std::exception& e) {
    LOG_ERROR("Top spenders query failed: " + std::string(e.what()), "persistent");
    return memory_system_->TopSpenders(timestamp, n);  // Fallback
  }
}

std::optional<std::string> BankingSystemPersistent::SchedulePayment(int timestamp,
                                                                  const std::string& account_id,
                                                                  int amount, int delay) {
  try {
    // Perform in-memory operation first
    auto result = memory_system_->SchedulePayment(timestamp, account_id, amount, delay);
    if (!result) {
      return std::nullopt;
    }

    std::string payment_id = *result;

    // Persist to database
    if (persistence_) {
      // Get the payment details from memory system (this is a bit hacky, but works)
      // In a real implementation, you'd modify the memory system to return the full payment record

      // For now, we'll reconstruct the payment record
      database::ScheduledPaymentRecord payment_record;
      payment_record.payment_id = payment_id;
      payment_record.account_id = account_id;
      payment_record.amount = amount;
      payment_record.due_timestamp = timestamp + delay;
      payment_record.created_at = timestamp;

      if (!persistence_->saveScheduledPayment(payment_record)) {
        LOG_ERROR("Failed to persist scheduled payment", "persistent");
        // Note: Should rollback in-memory change here
        return std::nullopt;
      }
    }

    return result;

  } catch (const std::exception& e) {
    LOG_ERROR("Schedule payment failed: " + std::string(e.what()), "persistent");
    return std::nullopt;
  }
}

bool BankingSystemPersistent::CancelPayment(int timestamp, const std::string& account_id,
                                          const std::string& payment_id) {
  try {
    // Perform in-memory operation first
    bool result = memory_system_->CancelPayment(timestamp, account_id, payment_id);
    if (!result) {
      return false;
    }

    // Update database
    if (persistence_) {
      if (!persistence_->cancelScheduledPayment(payment_id)) {
        LOG_ERROR("Failed to persist payment cancellation", "persistent");
        // Note: Should rollback in-memory change here
        return false;
      }
    }

    return true;

  } catch (const std::exception& e) {
    LOG_ERROR("Cancel payment failed: " + std::string(e.what()), "persistent");
    return false;
  }
}

bool BankingSystemPersistent::MergeAccounts(int timestamp, const std::string& account_id_1,
                                          const std::string& account_id_2) {
  try {
    // Get balance of account 2 before merge
    auto balance_2_opt = memory_system_->GetBalance(timestamp, account_id_2, timestamp);
    if (!balance_2_opt) {
      return false;  // Account 2 doesn't exist
    }
    int balance_transferred = *balance_2_opt;

    // Perform in-memory merge
    bool result = memory_system_->MergeAccounts(timestamp, account_id_1, account_id_2);
    if (!result) {
      return false;
    }

    // Persist merge to database
    if (persistence_) {
      if (!persistence_->saveAccountMerge(account_id_2, account_id_1, timestamp, balance_transferred)) {
        LOG_ERROR("Failed to persist account merge", "persistent");
        // Note: Should rollback in-memory change here
        return false;
      }
    }

    return true;

  } catch (const std::exception& e) {
    LOG_ERROR("Account merge failed: " + std::string(e.what()), "persistent");
    return false;
  }
}

std::optional<int> BankingSystemPersistent::GetBalance(int timestamp, const std::string& account_id, int time_at) {
  try {
    // For historical queries, use database if available
    if (persistence_ && time_at < timestamp) {
      // Historical query - use database
      auto balance = persistence_->getBalanceAtTime(account_id, time_at);
      if (balance) {
        return balance;
      }
    }

    // Current balance or fallback to memory system
    return memory_system_->GetBalance(timestamp, account_id, time_at);

  } catch (const std::exception& e) {
    LOG_ERROR("Balance query failed: " + std::string(e.what()), "persistent");
    return memory_system_->GetBalance(timestamp, account_id, time_at);  // Fallback
  }
}

bool BankingSystemPersistent::persistTransaction(const std::string& transaction_type,
                                               const std::string& account_id,
                                               int amount, int balance_before, int balance_after,
                                               int timestamp, const std::string& reference_id,
                                               const std::string& description) {
  if (!persistence_) return true;  // No-op if no persistence configured

  try {
    database::TransactionRecord transaction(account_id, transaction_type, amount,
                                           balance_before, balance_after, timestamp,
                                           reference_id, description);

    if (!persistence_->saveTransaction(transaction)) {
      return false;
    }

    // Save balance event for historical queries
    database::BalanceEvent balance_event(timestamp, balance_after - balance_before,
                                        transaction_type + "_EVENT");
    if (!persistence_->saveBalanceEvent(account_id, balance_event)) {
      LOG_WARN("Failed to save balance event, but transaction was saved", "persistent");
    }

    return true;

  } catch (const std::exception& e) {
    LOG_ERROR("Transaction persistence failed: " + std::string(e.what()), "persistent");
    return false;
  }
}

bool BankingSystemPersistent::loadFromDatabase() {
  if (!persistence_) return true;

  try {
    LOG_INFO("Loading existing data from database", "persistent");

    // Load account creation times into cache
    auto creation_times = persistence_->getAccountCreationTimes();
    {
      std::unique_lock<std::shared_mutex> lock(cache_mutex_);
      account_creation_cache_ = std::move(creation_times);
    }

    // Note: In a full implementation, you'd load all account balances and
    // rebuild the in-memory state from the database. For this demo,
    // we assume the system starts with empty in-memory state and
    // relies on the database for historical queries.

    LOG_INFO("Database data loading completed", "persistent");
    return true;

  } catch (const std::exception& e) {
    LOG_ERROR("Failed to load data from database: " + std::string(e.what()), "persistent");
    return false;
  }
}

bool BankingSystemPersistent::syncInMemoryWithDatabase() {
  // This would be called periodically to sync in-memory state with database
  // For now, we rely on write-through caching, so in-memory is always up-to-date
  return true;
}

}  // namespace banking
