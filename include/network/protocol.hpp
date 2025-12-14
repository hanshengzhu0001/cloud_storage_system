#ifndef PROTOCOL_HPP_
#define PROTOCOL_HPP_

#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <optional>
#include <string>
#include <vector>

namespace banking {
namespace network {
namespace protocol {

// Message types
enum class MessageType {
  CREATE_ACCOUNT,
  DEPOSIT,
  TRANSFER,
  GET_BALANCE,
  TOP_SPENDERS,
  SCHEDULE_PAYMENT,
  CANCEL_PAYMENT,
  MERGE_ACCOUNTS,
  AUTHENTICATE,
  HEARTBEAT,
  ERROR
};

// Response status
enum class Status {
  SUCCESS,
  ERROR,
  INVALID_REQUEST,
  UNAUTHORIZED,
  ACCOUNT_NOT_FOUND,
  INSUFFICIENT_FUNDS
};

// Request base structure
struct Request {
  MessageType type;
  int timestamp;
  std::string client_id;
  std::string session_token;
  std::map<std::string, std::string> payload;

  // Helper methods for specific request types
  static Request createAccount(int timestamp, const std::string& client_id,
                              const std::string& session_token,
                              const std::string& account_id);

  static Request deposit(int timestamp, const std::string& client_id,
                        const std::string& session_token,
                        const std::string& account_id, int amount);

  static Request transfer(int timestamp, const std::string& client_id,
                         const std::string& session_token,
                         const std::string& source_account,
                         const std::string& target_account, int amount);

  static Request getBalance(int timestamp, const std::string& client_id,
                           const std::string& session_token,
                           const std::string& account_id, int time_at);

  static Request topSpenders(int timestamp, const std::string& client_id,
                            const std::string& session_token, int n);

  static Request schedulePayment(int timestamp, const std::string& client_id,
                                const std::string& session_token,
                                const std::string& account_id, int amount, int delay);

  static Request cancelPayment(int timestamp, const std::string& client_id,
                              const std::string& session_token,
                              const std::string& account_id, const std::string& payment_id);

  static Request mergeAccounts(int timestamp, const std::string& client_id,
                              const std::string& session_token,
                              const std::string& account_id_1,
                              const std::string& account_id_2);

  static Request authenticate(int timestamp, const std::string& username,
                             const std::string& password);

  static Request heartbeat(int timestamp, const std::string& client_id);
};

// Response base structure
struct Response {
  Status status;
  std::string message;
  int timestamp;
  std::map<std::string, std::string> payload;

  // Helper methods for specific response types
  static Response success(const std::string& message, int timestamp,
                         const nlohmann::json& payload = nlohmann::json{});

  static Response error(Status status, const std::string& message, int timestamp);

  static Response accountCreated(const std::string& account_id, int timestamp);
  static Response depositResult(int new_balance, int timestamp);
  static Response transferResult(int new_source_balance, int timestamp);
  static Response balanceResult(int balance, int timestamp);
  static Response topSpendersResult(const std::vector<std::string>& spenders, int timestamp);
  static Response paymentScheduled(const std::string& payment_id, int timestamp);
  static Response paymentCancelled(int timestamp);
  static Response accountsMerged(int timestamp);
  static Response authenticated(const std::string& session_token, int timestamp);
};

// Serialization functions
std::string serializeRequest(const Request& request);
Request deserializeRequest(const std::string& json_str);

std::string serializeResponse(const Response& response);
Response deserializeResponse(const std::string& json_str);

// Message framing for TCP transport
class MessageFramer {
 public:
  static std::string frameMessage(const std::string& message);
  static std::string unframeMessage(const std::string& framed_message);
  static bool isCompleteMessage(const std::string& buffer);
};

}  // namespace protocol
}  // namespace network
}  // namespace banking

#endif  // PROTOCOL_HPP_
