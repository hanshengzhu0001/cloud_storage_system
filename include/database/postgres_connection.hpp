#ifndef POSTGRES_CONNECTION_HPP_
#define POSTGRES_CONNECTION_HPP_

#include <memory>
#include <string>
#include <mutex>
#include <postgresql/libpq-fe.h>

namespace banking {
namespace database {

/**
 * PostgreSQL database connection wrapper.
 * Handles connection management, connection pooling, and basic query execution.
 */
class PostgresConnection {
 public:
  /**
   * Connection configuration
   */
  struct Config {
    std::string host = "localhost";
    int port = 5432;
    std::string database = "banking_system";
    std::string username = "banking_user";
    std::string password = "";
    int connection_timeout = 30;  // seconds
    int max_connections = 10;
  };

  PostgresConnection(const Config& config);
  ~PostgresConnection();

  // Non-copyable
  PostgresConnection(const PostgresConnection&) = delete;
  PostgresConnection& operator=(const PostgresConnection&) = delete;

  /**
   * Connect to the database.
   */
  bool connect();

  /**
   * Disconnect from the database.
   */
  void disconnect();

  /**
   * Check if connected.
   */
  bool isConnected() const;

  /**
   * Execute a query that doesn't return results.
   */
  bool executeQuery(const std::string& query);

  /**
   * Execute a query and get results.
   */
  PGresult* executeQueryWithResult(const std::string& query);

  /**
   * Execute a parameterized query.
   */
  PGresult* executeParameterizedQuery(const std::string& query,
                                     int nParams,
                                     const char* const* paramValues,
                                     const int* paramLengths = nullptr,
                                     const int* paramFormats = nullptr);

  /**
   * Begin a transaction.
   */
  bool beginTransaction();

  /**
   * Commit a transaction.
   */
  bool commitTransaction();

  /**
   * Rollback a transaction.
   */
  bool rollbackTransaction();

  /**
   * Get last error message.
   */
  std::string getLastError() const;

  /**
   * Get connection info for logging.
   */
  std::string getConnectionInfo() const;

 private:
  Config config_;
  PGconn* connection_;
  mutable std::mutex mutex_;
  bool in_transaction_;
};

/**
 * RAII wrapper for database transactions.
 */
class TransactionGuard {
 public:
  explicit TransactionGuard(PostgresConnection& conn);
  ~TransactionGuard();

  // Non-copyable
  TransactionGuard(const TransactionGuard&) = delete;
  TransactionGuard& operator=(const TransactionGuard&) = delete;

  /**
   * Commit the transaction (called automatically in destructor).
   */
  void commit();

  /**
   * Rollback the transaction.
   */
  void rollback();

 private:
  PostgresConnection& conn_;
  bool committed_;
};

}  // namespace database
}  // namespace banking

#endif  // POSTGRES_CONNECTION_HPP_
