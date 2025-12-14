#include "banking_persistence.hpp"

#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <algorithm>

namespace banking {
namespace database {

BankingPersistence::BankingPersistence(std::shared_ptr<PostgresConnection> conn)
    : conn_(conn) {
}

bool BankingPersistence::initializeSchema() {
  try {
    // Execute schema file
    if (!executeSchemaFile("database/schema.sql")) {
      std::cerr << "Failed to execute schema file" << std::endl;
      return false;
    }

    // Log initialization
    logSystemEvent("DATABASE_INIT", "INFO", "Database schema initialized successfully", "persistence");

    return true;
  } catch (const std::exception& e) {
    std::cerr << "Schema initialization failed: " << e.what() << std::endl;
    return false;
  }
}

bool BankingPersistence::createAccount(const std::string& account_id, int initial_balance) {
  try {
    const char* paramValues[3] = {
      account_id.c_str(),
      std::to_string(initial_balance).c_str(),
      account_id.c_str()  // for balance event
    };

    TransactionGuard transaction(*conn_);

    // Insert account
    std::string query = R"(
      INSERT INTO accounts (account_id, balance)
      VALUES ($1, $2)
      ON CONFLICT (account_id) DO NOTHING
    )";

    auto result = conn_->executeParameterizedQuery(query, 2, paramValues);
    if (!result) return false;
    PQclear(result);

    // Insert balance event for creation
    std::string eventQuery = R"(
      INSERT INTO balance_events (account_id, timestamp, balance_delta, event_type)
      VALUES ($1, CURRENT_TIMESTAMP, $2, 'CREATION')
    )";

    const char* eventParams[2] = {account_id.c_str(), std::to_string(initial_balance).c_str()};
    result = conn_->executeParameterizedQuery(eventQuery, 2, eventParams);
    if (!result) return false;
    PQclear(result);

    transaction.commit();

    logSystemEvent("ACCOUNT_CREATED", "INFO",
                  "Account created: " + account_id + " with balance " + std::to_string(initial_balance),
                  "persistence");

    return true;
  } catch (const std::exception& e) {
    std::cerr << "Failed to create account " << account_id << ": " << e.what() << std::endl;
    return false;
  }
}

bool BankingPersistence::accountExists(const std::string& account_id) {
  try {
    std::string query = "SELECT 1 FROM accounts WHERE account_id = $1 AND is_active = TRUE";

    const char* paramValues[1] = {account_id.c_str()};
    auto result = conn_->executeParameterizedQuery(query, 1, paramValues);

    if (!result) return false;

    bool exists = PQntuples(result) > 0;
    PQclear(result);

    return exists;
  } catch (const std::exception& e) {
    std::cerr << "Failed to check account existence: " << e.what() << std::endl;
    return false;
  }
}

std::optional<int> BankingPersistence::getAccountBalance(const std::string& account_id) {
  try {
    std::string query = "SELECT balance FROM accounts WHERE account_id = $1 AND is_active = TRUE";

    const char* paramValues[1] = {account_id.c_str()};
    auto result = conn_->executeParameterizedQuery(query, 1, paramValues);

    if (!result || PQntuples(result) == 0) {
      if (result) PQclear(result);
      return std::nullopt;
    }

    int balance = std::stoi(PQgetvalue(result, 0, 0));
    PQclear(result);

    return balance;
  } catch (const std::exception& e) {
    std::cerr << "Failed to get account balance: " << e.what() << std::endl;
    return std::nullopt;
  }
}

bool BankingPersistence::updateAccountBalance(const std::string& account_id, int new_balance) {
  try {
    std::string query = R"(
      UPDATE accounts
      SET balance = $2, updated_at = CURRENT_TIMESTAMP
      WHERE account_id = $1 AND is_active = TRUE
    )";

    const char* paramValues[2] = {
      account_id.c_str(),
      std::to_string(new_balance).c_str()
    };

    auto result = conn_->executeParameterizedQuery(query, 2, paramValues);
    if (!result) return false;

    bool updated = std::string(PQcmdTuples(result)) != "0";
    PQclear(result);

    if (updated) {
      logSystemEvent("BALANCE_UPDATED", "INFO",
                    "Account " + account_id + " balance updated to " + std::to_string(new_balance),
                    "persistence");
    }

    return updated;
  } catch (const std::exception& e) {
    std::cerr << "Failed to update account balance: " << e.what() << std::endl;
    return false;
  }
}

bool BankingPersistence::saveTransaction(const TransactionRecord& transaction) {
  try {
    std::string query = R"(
      INSERT INTO transactions (
        id, account_id, transaction_type, amount,
        balance_before, balance_after, timestamp,
        reference_id, description, metadata
      ) VALUES (
        COALESCE($1, uuid_generate_v4()), $2, $3::transaction_type, $4,
        $5, $6, TO_TIMESTAMP($7), $8, $9, $10::jsonb
      )
    )";

    // Convert metadata to JSON string
    std::stringstream metadata_ss;
    metadata_ss << "{";
    bool first = true;
    for (const auto& [key, value] : transaction.metadata) {
      if (!first) metadata_ss << ",";
      metadata_ss << "\"" << key << "\":\"" << value << "\"";
      first = false;
    }
    metadata_ss << "}";

    const char* paramValues[10] = {
      transaction.id.empty() ? nullptr : transaction.id.c_str(),
      transaction.account_id.c_str(),
      transaction.transaction_type.c_str(),
      std::to_string(transaction.amount).c_str(),
      std::to_string(transaction.balance_before).c_str(),
      std::to_string(transaction.balance_after).c_str(),
      std::to_string(transaction.timestamp).c_str(),
      transaction.reference_id.empty() ? nullptr : transaction.reference_id.c_str(),
      transaction.description.empty() ? nullptr : transaction.description.c_str(),
      metadata_ss.str().c_str()
    };

    auto result = conn_->executeParameterizedQuery(query, 10, paramValues);
    if (!result) return false;
    PQclear(result);

    return true;
  } catch (const std::exception& e) {
    std::cerr << "Failed to save transaction: " << e.what() << std::endl;
    return false;
  }
}

std::vector<TransactionRecord> BankingPersistence::getAccountTransactions(const std::string& account_id,
                                                                        int limit, int offset) {
  std::vector<TransactionRecord> transactions;

  try {
    std::string query = R"(
      SELECT id, account_id, transaction_type::text, amount,
             balance_before, balance_after,
             EXTRACT(epoch FROM timestamp)::int as timestamp,
             COALESCE(reference_id, '') as reference_id,
             COALESCE(description, '') as description
      FROM transactions
      WHERE account_id = $1
      ORDER BY timestamp DESC
      LIMIT $2 OFFSET $3
    )";

    const char* paramValues[3] = {
      account_id.c_str(),
      std::to_string(limit).c_str(),
      std::to_string(offset).c_str()
    };

    auto result = conn_->executeParameterizedQuery(query, 3, paramValues);
    if (!result) return transactions;

    int numRows = PQntuples(result);
    for (int i = 0; i < numRows; ++i) {
      TransactionRecord tx;
      tx.id = PQgetvalue(result, i, 0);
      tx.account_id = PQgetvalue(result, i, 1);
      tx.transaction_type = PQgetvalue(result, i, 2);
      tx.amount = std::stoi(PQgetvalue(result, i, 3));
      tx.balance_before = std::stoi(PQgetvalue(result, i, 4));
      tx.balance_after = std::stoi(PQgetvalue(result, i, 5));
      tx.timestamp = std::stoi(PQgetvalue(result, i, 6));
      tx.reference_id = PQgetvalue(result, i, 7);
      tx.description = PQgetvalue(result, i, 8);

      transactions.push_back(tx);
    }

    PQclear(result);
  } catch (const std::exception& e) {
    std::cerr << "Failed to get account transactions: " << e.what() << std::endl;
  }

  return transactions;
}

std::optional<int> BankingPersistence::getAccountOutgoingTotal(const std::string& account_id) {
  try {
    std::string query = R"(
      SELECT COALESCE(SUM(amount), 0)::int as total_outgoing
      FROM transactions
      WHERE account_id = $1
      AND transaction_type IN ('WITHDRAWAL', 'TRANSFER_SEND', 'PAYMENT_PROCESSED')
    )";

    const char* paramValues[1] = {account_id.c_str()};
    auto result = conn_->executeParameterizedQuery(query, 1, paramValues);

    if (!result || PQntuples(result) == 0) {
      if (result) PQclear(result);
      return std::nullopt;
    }

    int total = std::stoi(PQgetvalue(result, 0, 0));
    PQclear(result);

    return total;
  } catch (const std::exception& e) {
    std::cerr << "Failed to get outgoing total: " << e.what() << std::endl;
    return std::nullopt;
  }
}

bool BankingPersistence::saveScheduledPayment(const ScheduledPaymentRecord& payment) {
  try {
    std::string query = R"(
      INSERT INTO scheduled_payments (
        payment_id, account_id, amount, due_timestamp, creation_order
      ) VALUES ($1, $2, $3, TO_TIMESTAMP($4), $5)
      ON CONFLICT (payment_id) DO NOTHING
    )";

    const char* paramValues[5] = {
      payment.payment_id.c_str(),
      payment.account_id.c_str(),
      std::to_string(payment.amount).c_str(),
      std::to_string(payment.due_timestamp).c_str(),
      std::to_string(payment.creation_order).c_str()
    };

    auto result = conn_->executeParameterizedQuery(query, 5, paramValues);
    if (!result) return false;
    PQclear(result);

    return true;
  } catch (const std::exception& e) {
    std::cerr << "Failed to save scheduled payment: " << e.what() << std::endl;
    return false;
  }
}

std::optional<ScheduledPaymentRecord> BankingPersistence::getScheduledPayment(const std::string& payment_id) {
  try {
    std::string query = R"(
      SELECT payment_id, account_id, amount,
             EXTRACT(epoch FROM due_timestamp)::int as due_timestamp,
             EXTRACT(epoch FROM created_at)::int as created_at,
             is_canceled, is_processed,
             CASE WHEN processing_timestamp IS NOT NULL
                  THEN EXTRACT(epoch FROM processing_timestamp)::int
                  ELSE 0 END as processing_timestamp,
             creation_order
      FROM scheduled_payments
      WHERE payment_id = $1
    )";

    const char* paramValues[1] = {payment_id.c_str()};
    auto result = conn_->executeParameterizedQuery(query, 1, paramValues);

    if (!result || PQntuples(result) == 0) {
      if (result) PQclear(result);
      return std::nullopt;
    }

    ScheduledPaymentRecord payment;
    payment.payment_id = PQgetvalue(result, 0, 0);
    payment.account_id = PQgetvalue(result, 0, 1);
    payment.amount = std::stoi(PQgetvalue(result, 0, 2));
    payment.due_timestamp = std::stoi(PQgetvalue(result, 0, 3));
    payment.created_at = std::stoi(PQgetvalue(result, 0, 4));
    payment.is_canceled = std::string(PQgetvalue(result, 0, 5)) == "t";
    payment.is_processed = std::string(PQgetvalue(result, 0, 6)) == "t";
    payment.processing_timestamp = std::stoi(PQgetvalue(result, 0, 7));
    payment.creation_order = std::stoi(PQgetvalue(result, 0, 8));

    PQclear(result);
    return payment;
  } catch (const std::exception& e) {
    std::cerr << "Failed to get scheduled payment: " << e.what() << std::endl;
    return std::nullopt;
  }
}

bool BankingPersistence::updateScheduledPayment(const std::string& payment_id, bool is_processed,
                                              int processing_timestamp) {
  try {
    std::string query;
    const char* paramValues[3];

    if (is_processed) {
      query = R"(
        UPDATE scheduled_payments
        SET is_processed = TRUE, processing_timestamp = TO_TIMESTAMP($2)
        WHERE payment_id = $1 AND NOT is_processed
      )";
      paramValues[0] = payment_id.c_str();
      paramValues[1] = std::to_string(processing_timestamp).c_str();
      paramValues[2] = nullptr;
    } else {
      query = R"(
        UPDATE scheduled_payments
        SET is_canceled = TRUE
        WHERE payment_id = $1 AND NOT is_processed AND NOT is_canceled
      )";
      paramValues[0] = payment_id.c_str();
      paramValues[1] = nullptr;
      paramValues[2] = nullptr;
    }

    auto result = conn_->executeParameterizedQuery(query, is_processed ? 2 : 1, paramValues);
    if (!result) return false;

    bool updated = std::string(PQcmdTuples(result)) != "0";
    PQclear(result);

    return updated;
  } catch (const std::exception& e) {
    std::cerr << "Failed to update scheduled payment: " << e.what() << std::endl;
    return false;
  }
}

std::vector<ScheduledPaymentRecord> BankingPersistence::getDuePayments(int current_timestamp) {
  std::vector<ScheduledPaymentRecord> payments;

  try {
    std::string query = R"(
      SELECT payment_id, account_id, amount,
             EXTRACT(epoch FROM due_timestamp)::int as due_timestamp,
             EXTRACT(epoch FROM created_at)::int as created_at,
             is_canceled, is_processed, creation_order
      FROM scheduled_payments
      WHERE due_timestamp <= TO_TIMESTAMP($1)
      AND NOT is_canceled
      AND NOT is_processed
      ORDER BY creation_order
    )";

    const char* paramValues[1] = {std::to_string(current_timestamp).c_str()};
    auto result = conn_->executeParameterizedQuery(query, 1, paramValues);

    if (!result) return payments;

    int numRows = PQntuples(result);
    for (int i = 0; i < numRows; ++i) {
      ScheduledPaymentRecord payment;
      payment.payment_id = PQgetvalue(result, i, 0);
      payment.account_id = PQgetvalue(result, i, 1);
      payment.amount = std::stoi(PQgetvalue(result, i, 2));
      payment.due_timestamp = std::stoi(PQgetvalue(result, i, 3));
      payment.created_at = std::stoi(PQgetvalue(result, i, 4));
      payment.is_canceled = std::string(PQgetvalue(result, i, 5)) == "t";
      payment.is_processed = std::string(PQgetvalue(result, i, 6)) == "t";
      payment.creation_order = std::stoi(PQgetvalue(result, i, 7));

      payments.push_back(payment);
    }

    PQclear(result);
  } catch (const std::exception& e) {
    std::cerr << "Failed to get due payments: " << e.what() << std::endl;
  }

  return payments;
}

bool BankingPersistence::cancelScheduledPayment(const std::string& payment_id) {
  return updateScheduledPayment(payment_id, false);
}

bool BankingPersistence::saveBalanceEvent(const std::string& account_id, const BalanceEvent& event) {
  try {
    std::string query = R"(
      INSERT INTO balance_events (account_id, timestamp, balance_delta, event_type)
      VALUES ($1, TO_TIMESTAMP($2), $3, $4)
    )";

    const char* paramValues[4] = {
      account_id.c_str(),
      std::to_string(event.timestamp).c_str(),
      std::to_string(event.balance_delta).c_str(),
      event.event_type.c_str()
    };

    auto result = conn_->executeParameterizedQuery(query, 4, paramValues);
    if (!result) return false;
    PQclear(result);

    return true;
  } catch (const std::exception& e) {
    std::cerr << "Failed to save balance event: " << e.what() << std::endl;
    return false;
  }
}

std::vector<BalanceEvent> BankingPersistence::getBalanceEvents(const std::string& account_id,
                                                             int start_time, int end_time) {
  std::vector<BalanceEvent> events;

  try {
    std::string query = R"(
      SELECT EXTRACT(epoch FROM timestamp)::int as timestamp,
             balance_delta, event_type
      FROM balance_events
      WHERE account_id = $1
      AND timestamp >= TO_TIMESTAMP($2)
      AND timestamp <= TO_TIMESTAMP($3)
      ORDER BY timestamp
    )";

    const char* paramValues[3] = {
      account_id.c_str(),
      std::to_string(start_time).c_str(),
      std::to_string(end_time).c_str()
    };

    auto result = conn_->executeParameterizedQuery(query, 3, paramValues);
    if (!result) return events;

    int numRows = PQntuples(result);
    for (int i = 0; i < numRows; ++i) {
      BalanceEvent event;
      event.timestamp = std::stoi(PQgetvalue(result, i, 0));
      event.balance_delta = std::stoi(PQgetvalue(result, i, 1));
      event.event_type = PQgetvalue(result, i, 2);

      events.push_back(event);
    }

    PQclear(result);
  } catch (const std::exception& e) {
    std::cerr << "Failed to get balance events: " << e.what() << std::endl;
  }

  return events;
}

std::optional<int> BankingPersistence::getBalanceAtTime(const std::string& account_id, int time_at) {
  try {
    // Get the resolved account ID at the given time
    std::string resolved_account = resolveAccountAtTime(account_id, time_at);

    std::string query = R"(
      SELECT SUM(balance_delta) as balance
      FROM balance_events
      WHERE account_id = $1
      AND timestamp <= TO_TIMESTAMP($2)
    )";

    const char* paramValues[2] = {
      resolved_account.c_str(),
      std::to_string(time_at).c_str()
    };

    auto result = conn_->executeParameterizedQuery(query, 2, paramValues);
    if (!result || PQntuples(result) == 0) {
      if (result) PQclear(result);
      return std::nullopt;
    }

    // Handle NULL result
    const char* balance_str = PQgetvalue(result, 0, 0);
    if (balance_str && strlen(balance_str) > 0) {
      int balance = std::stoi(balance_str);
      PQclear(result);
      return balance;
    }

    PQclear(result);
    return 0;  // No transactions yet
  } catch (const std::exception& e) {
    std::cerr << "Failed to get balance at time: " << e.what() << std::endl;
    return std::nullopt;
  }
}

bool BankingPersistence::saveAccountMerge(const std::string& child_account_id,
                                        const std::string& parent_account_id,
                                        int merge_timestamp, int balance_transferred) {
  try {
    std::string query = R"(
      INSERT INTO account_merges (child_account_id, parent_account_id, merge_timestamp, balance_transferred)
      VALUES ($1, $2, TO_TIMESTAMP($3), $4)
    )";

    const char* paramValues[4] = {
      child_account_id.c_str(),
      parent_account_id.c_str(),
      std::to_string(merge_timestamp).c_str(),
      std::to_string(balance_transferred).c_str()
    };

    auto result = conn_->executeParameterizedQuery(query, 4, paramValues);
    if (!result) return false;
    PQclear(result);

    // Mark child account as inactive
    std::string deactivateQuery = "UPDATE accounts SET is_active = FALSE WHERE account_id = $1";
    const char* deactivateParams[1] = {child_account_id.c_str()};
    result = conn_->executeParameterizedQuery(deactivateQuery, 1, deactivateParams);
    if (!result) return false;
    PQclear(result);

    return true;
  } catch (const std::exception& e) {
    std::cerr << "Failed to save account merge: " << e.what() << std::endl;
    return false;
  }
}

std::optional<std::pair<std::string, int>> BankingPersistence::getAccountMergeInfo(const std::string& account_id) {
  try {
    std::string query = R"(
      SELECT parent_account_id, EXTRACT(epoch FROM merge_timestamp)::int as merge_timestamp
      FROM account_merges
      WHERE child_account_id = $1
      ORDER BY merge_timestamp DESC
      LIMIT 1
    )";

    const char* paramValues[1] = {account_id.c_str()};
    auto result = conn_->executeParameterizedQuery(query, 1, paramValues);

    if (!result || PQntuples(result) == 0) {
      if (result) PQclear(result);
      return std::nullopt;
    }

    std::string parent_id = PQgetvalue(result, 0, 0);
    int merge_timestamp = std::stoi(PQgetvalue(result, 0, 1));

    PQclear(result);
    return std::make_pair(parent_id, merge_timestamp);
  } catch (const std::exception& e) {
    std::cerr << "Failed to get account merge info: " << e.what() << std::endl;
    return std::nullopt;
  }
}

std::string BankingPersistence::resolveAccountAtTime(const std::string& account_id, int time_at) {
  auto merge_info = getAccountMergeInfo(account_id);
  if (merge_info && merge_info->second <= time_at) {
    // Account was merged before or at the requested time, follow the chain
    return resolveAccountAtTime(merge_info->first, time_at);
  }
  return account_id;
}

std::vector<std::pair<std::string, int>> BankingPersistence::getTopSpenders(int limit) {
  std::vector<std::pair<std::string, int>> spenders;

  try {
    std::string query = R"(
      SELECT account_id, total_outgoing
      FROM account_summary
      WHERE is_active = TRUE
      ORDER BY total_outgoing DESC, account_id ASC
      LIMIT $1
    )";

    const char* paramValues[1] = {std::to_string(limit).c_str()};
    auto result = conn_->executeParameterizedQuery(query, 1, paramValues);

    if (!result) return spenders;

    int numRows = PQntuples(result);
    for (int i = 0; i < numRows; ++i) {
      std::string account_id = PQgetvalue(result, i, 0);
      int total_outgoing = std::stoi(PQgetvalue(result, i, 1));
      spenders.emplace_back(account_id, total_outgoing);
    }

    PQclear(result);
  } catch (const std::exception& e) {
    std::cerr << "Failed to get top spenders: " << e.what() << std::endl;
  }

  return spenders;
}

std::map<std::string, int> BankingPersistence::getAccountCreationTimes() {
  std::map<std::string, int> creation_times;

  try {
    std::string query = "SELECT account_id, EXTRACT(epoch FROM created_at)::int as created_at FROM accounts";

    auto result = conn_->executeQueryWithResult(query);
    if (!result) return creation_times;

    int numRows = PQntuples(result);
    for (int i = 0; i < numRows; ++i) {
      std::string account_id = PQgetvalue(result, i, 0);
      int created_at = std::stoi(PQgetvalue(result, i, 1));
      creation_times[account_id] = created_at;
    }

    PQclear(result);
  } catch (const std::exception& e) {
    std::cerr << "Failed to get account creation times: " << e.what() << std::endl;
  }

  return creation_times;
}

bool BankingPersistence::saveFraudAlert(const std::string& account_id, const std::string& transaction_id,
                                       double risk_score, const std::vector<std::string>& risk_factors,
                                       const std::string& recommendation, int confidence_level) {
  try {
    // Convert risk factors array to PostgreSQL array syntax
    std::stringstream factors_ss;
    factors_ss << "{";
    for (size_t i = 0; i < risk_factors.size(); ++i) {
      if (i > 0) factors_ss << ",";
      factors_ss << "\"" << risk_factors[i] << "\"";
    }
    factors_ss << "}";

    std::string query = R"(
      INSERT INTO fraud_alerts (
        account_id, transaction_id, risk_score, risk_factors,
        recommendation, confidence_level
      ) VALUES ($1, $2, $3, $4, $5, $6)
    )";

    const char* paramValues[6] = {
      account_id.c_str(),
      transaction_id.empty() ? nullptr : transaction_id.c_str(),
      std::to_string(risk_score).c_str(),
      factors_ss.str().c_str(),
      recommendation.c_str(),
      std::to_string(confidence_level).c_str()
    };

    auto result = conn_->executeParameterizedQuery(query, 6, paramValues);
    if (!result) return false;
    PQclear(result);

    return true;
  } catch (const std::exception& e) {
    std::cerr << "Failed to save fraud alert: " << e.what() << std::endl;
    return false;
  }
}

bool BankingPersistence::logSystemEvent(const std::string& event_type, const std::string& severity,
                                       const std::string& message, const std::string& component,
                                       const std::string& correlation_id) {
  try {
    std::string query = R"(
      INSERT INTO system_events (event_type, severity, message, component, correlation_id)
      VALUES ($1, $2, $3, $4, $5)
    )";

    const char* paramValues[5] = {
      event_type.c_str(),
      severity.c_str(),
      message.c_str(),
      component.empty() ? nullptr : component.c_str(),
      correlation_id.empty() ? nullptr : correlation_id.c_str()
    };

    auto result = conn_->executeParameterizedQuery(query, 5, paramValues);
    if (!result) return false;
    PQclear(result);

    return true;
  } catch (const std::exception& e) {
    std::cerr << "Failed to log system event: " << e.what() << std::endl;
    return false;
  }
}

bool BankingPersistence::executeSchemaFile(const std::string& schema_path) {
  try {
    std::ifstream schema_file(schema_path);
    if (!schema_file.is_open()) {
      std::cerr << "Could not open schema file: " << schema_path << std::endl;
      return false;
    }

    std::stringstream buffer;
    buffer << schema_file.rdbuf();
    std::string schema_sql = buffer.str();

    // Split by semicolon and execute each statement
    std::stringstream ss(schema_sql);
    std::string statement;
    size_t pos = 0;
    std::string delimiter = ";";

    while ((pos = schema_sql.find(delimiter, pos)) != std::string::npos) {
      std::string stmt = schema_sql.substr(0, pos);
      if (!stmt.empty() && std::any_of(stmt.begin(), stmt.end(), ::isalnum)) {
        if (!conn_->executeQuery(stmt)) {
          std::cerr << "Failed to execute schema statement: " << stmt.substr(0, 100) << "..." << std::endl;
          return false;
        }
      }
      schema_sql.erase(0, pos + delimiter.length());
      pos = 0;
    }

    return true;
  } catch (const std::exception& e) {
    std::cerr << "Schema execution failed: " << e.what() << std::endl;
    return false;
  }
}

}  // namespace database
}  // namespace banking
