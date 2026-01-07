#pragma once

#include <chrono>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace core_engine {
namespace audit {

// Audit event types
enum class EventType {
  kLogin,
  kLogout,
  kLoginFailed,
  kPut,
  kGet,
  kDelete,
  kBatchOperation,
  kExport,
  kClearDatabase,
  kConfigChange,
  kUserCreated,
  kUserDeactivated,
  kSessionExpired,
  kUnauthorizedAccess
};

// Audit log entry
struct AuditEntry {
  std::chrono::system_clock::time_point timestamp;
  EventType event_type;
  std::string username;
  std::string ip_address;
  std::string details;
  bool success{true};
  
  std::string ToString() const;
  std::string ToJSON() const;
};

// Audit logger - thread-safe audit trail
class AuditLogger {
 public:
  explicit AuditLogger(const std::string& log_file_path);
  ~AuditLogger();

  // Log different event types
  void LogLogin(const std::string& username, const std::string& ip, bool success);
  void LogLogout(const std::string& username, const std::string& ip);
  void LogPut(const std::string& username, const std::string& key, bool success);
  void LogGet(const std::string& username, const std::string& key, bool success);
  void LogDelete(const std::string& username, const std::string& key, bool success);
  void LogBatchOperation(const std::string& username, size_t count, bool success);
  void LogExport(const std::string& username, size_t entry_count);
  void LogClearDatabase(const std::string& username, size_t deleted_count);
  void LogUnauthorizedAccess(const std::string& username, const std::string& ip,
                              const std::string& attempted_action);
  
  // General logging
  void Log(const AuditEntry& entry);
  void Log(EventType type, const std::string& username, const std::string& ip,
           const std::string& details, bool success = true);
  
  // Querying audit log
  std::vector<AuditEntry> GetRecentEntries(size_t count) const;
  std::vector<AuditEntry> GetEntriesByUser(const std::string& username, size_t max_count = 100) const;
  std::vector<AuditEntry> GetEntriesByType(EventType type, size_t max_count = 100) const;
  std::vector<AuditEntry> GetEntriesInTimeRange(
      std::chrono::system_clock::time_point start,
      std::chrono::system_clock::time_point end) const;
  
  // Statistics
  size_t GetTotalEntryCount() const;
  size_t GetFailedLoginCount() const;
  size_t GetUnauthorizedAccessCount() const;
  
  // File management
  void Flush();
  void Rotate();  // Rotate log file when it gets too large

 private:
  std::string EventTypeToString(EventType type) const;
  void WriteToFile(const AuditEntry& entry);
  void LoadExistingEntries();

  std::string log_file_path_;
  std::ofstream log_file_;
  mutable std::mutex mutex_;
  std::vector<AuditEntry> entries_;  // In-memory cache for querying
  static constexpr size_t kMaxCachedEntries = 10000;
  static constexpr size_t kMaxFileSize = 100 * 1024 * 1024;  // 100 MB
};

}  // namespace audit
}  // namespace core_engine
