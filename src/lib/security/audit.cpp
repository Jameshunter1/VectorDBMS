#include <core_engine/security/audit.hpp>

#include <algorithm>
#include <filesystem>
#include <iomanip>

namespace fs = std::filesystem;

namespace core_engine {
namespace audit {

namespace {

std::string TimeToString(std::chrono::system_clock::time_point tp) {
  auto time_t = std::chrono::system_clock::to_time_t(tp);
  std::ostringstream oss;
  oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
  return oss.str();
}

std::string EscapeJSON(const std::string& input) {
  std::string output;
  for (char c : input) {
    switch (c) {
    case '"':
      output += "\\\"";
      break;
    case '\\':
      output += "\\\\";
      break;
    case '\n':
      output += "\\n";
      break;
    case '\r':
      output += "\\r";
      break;
    case '\t':
      output += "\\t";
      break;
    default:
      output += c;
    }
  }
  return output;
}

} // namespace

std::string AuditEntry::ToString() const {
  std::ostringstream oss;
  oss << "[" << TimeToString(timestamp) << "] " << (success ? "SUCCESS" : "FAILURE") << " "
      << "User:" << username << " "
      << "IP:" << ip_address << " "
      << "Details:" << details;
  return oss.str();
}

std::string AuditEntry::ToJSON() const {
  std::ostringstream oss;
  oss << "{"
      << "\"timestamp\":\"" << TimeToString(timestamp) << "\","
      << "\"success\":" << (success ? "true" : "false") << ","
      << "\"username\":\"" << EscapeJSON(username) << "\","
      << "\"ip_address\":\"" << EscapeJSON(ip_address) << "\","
      << "\"details\":\"" << EscapeJSON(details) << "\""
      << "}";
  return oss.str();
}

AuditLogger::AuditLogger(const std::string& log_file_path) : log_file_path_(log_file_path) {
  // Create directory if it doesn't exist
  fs::path log_path(log_file_path_);
  if (log_path.has_parent_path()) {
    fs::create_directories(log_path.parent_path());
  }

  // Open log file in append mode
  log_file_.open(log_file_path_, std::ios::app);

  // Load existing entries for querying
  LoadExistingEntries();
}

AuditLogger::~AuditLogger() {
  Flush();
  if (log_file_.is_open()) {
    log_file_.close();
  }
}

void AuditLogger::LogLogin(const std::string& username, const std::string& ip, bool success) {
  AuditEntry entry;
  entry.timestamp = std::chrono::system_clock::now();
  entry.event_type = success ? EventType::kLogin : EventType::kLoginFailed;
  entry.username = username;
  entry.ip_address = ip;
  entry.details = success ? "User logged in" : "Login failed";
  entry.success = success;
  Log(entry);
}

void AuditLogger::LogLogout(const std::string& username, const std::string& ip) {
  AuditEntry entry;
  entry.timestamp = std::chrono::system_clock::now();
  entry.event_type = EventType::kLogout;
  entry.username = username;
  entry.ip_address = ip;
  entry.details = "User logged out";
  entry.success = true;
  Log(entry);
}

void AuditLogger::LogPut(const std::string& username, const std::string& key, bool success) {
  AuditEntry entry;
  entry.timestamp = std::chrono::system_clock::now();
  entry.event_type = EventType::kPut;
  entry.username = username;
  entry.details = "PUT key: " + key;
  entry.success = success;
  Log(entry);
}

void AuditLogger::LogGet(const std::string& username, const std::string& key, bool success) {
  AuditEntry entry;
  entry.timestamp = std::chrono::system_clock::now();
  entry.event_type = EventType::kGet;
  entry.username = username;
  entry.details = "GET key: " + key;
  entry.success = success;
  Log(entry);
}

void AuditLogger::LogDelete(const std::string& username, const std::string& key, bool success) {
  AuditEntry entry;
  entry.timestamp = std::chrono::system_clock::now();
  entry.event_type = EventType::kDelete;
  entry.username = username;
  entry.details = "DELETE key: " + key;
  entry.success = success;
  Log(entry);
}

void AuditLogger::LogBatchOperation(const std::string& username, size_t count, bool success) {
  AuditEntry entry;
  entry.timestamp = std::chrono::system_clock::now();
  entry.event_type = EventType::kBatchOperation;
  entry.username = username;
  entry.details = "Batch operation: " + std::to_string(count) + " entries";
  entry.success = success;
  Log(entry);
}

void AuditLogger::LogExport(const std::string& username, size_t entry_count) {
  AuditEntry entry;
  entry.timestamp = std::chrono::system_clock::now();
  entry.event_type = EventType::kExport;
  entry.username = username;
  entry.details = "Exported " + std::to_string(entry_count) + " entries";
  entry.success = true;
  Log(entry);
}

void AuditLogger::LogClearDatabase(const std::string& username, size_t deleted_count) {
  AuditEntry entry;
  entry.timestamp = std::chrono::system_clock::now();
  entry.event_type = EventType::kClearDatabase;
  entry.username = username;
  entry.details = "Cleared database: " + std::to_string(deleted_count) + " entries deleted";
  entry.success = true;
  Log(entry);
}

void AuditLogger::LogUnauthorizedAccess(const std::string& username, const std::string& ip,
                                        const std::string& attempted_action) {
  AuditEntry entry;
  entry.timestamp = std::chrono::system_clock::now();
  entry.event_type = EventType::kUnauthorizedAccess;
  entry.username = username;
  entry.ip_address = ip;
  entry.details = "Unauthorized: " + attempted_action;
  entry.success = false;
  Log(entry);
}

void AuditLogger::Log(const AuditEntry& entry) {
  std::lock_guard<std::mutex> lock(mutex_);

  // Add to in-memory cache
  entries_.push_back(entry);
  if (entries_.size() > kMaxCachedEntries) {
    entries_.erase(entries_.begin());
  }

  // Write to file
  WriteToFile(entry);
}

void AuditLogger::Log(EventType type, const std::string& username, const std::string& ip,
                      const std::string& details, bool success) {
  AuditEntry entry;
  entry.timestamp = std::chrono::system_clock::now();
  entry.event_type = type;
  entry.username = username;
  entry.ip_address = ip;
  entry.details = details;
  entry.success = success;
  Log(entry);
}

std::vector<AuditEntry> AuditLogger::GetRecentEntries(size_t count) const {
  std::lock_guard<std::mutex> lock(mutex_);

  size_t start = entries_.size() > count ? entries_.size() - count : 0;
  return std::vector<AuditEntry>(entries_.begin() + start, entries_.end());
}

std::vector<AuditEntry> AuditLogger::GetEntriesByUser(const std::string& username,
                                                      size_t max_count) const {
  std::lock_guard<std::mutex> lock(mutex_);

  std::vector<AuditEntry> result;
  for (auto it = entries_.rbegin(); it != entries_.rend() && result.size() < max_count; ++it) {
    if (it->username == username) {
      result.push_back(*it);
    }
  }
  return result;
}

std::vector<AuditEntry> AuditLogger::GetEntriesByType(EventType type, size_t max_count) const {
  std::lock_guard<std::mutex> lock(mutex_);

  std::vector<AuditEntry> result;
  for (auto it = entries_.rbegin(); it != entries_.rend() && result.size() < max_count; ++it) {
    if (it->event_type == type) {
      result.push_back(*it);
    }
  }
  return result;
}

std::vector<AuditEntry>
AuditLogger::GetEntriesInTimeRange(std::chrono::system_clock::time_point start,
                                   std::chrono::system_clock::time_point end) const {
  std::lock_guard<std::mutex> lock(mutex_);

  std::vector<AuditEntry> result;
  for (const auto& entry : entries_) {
    if (entry.timestamp >= start && entry.timestamp <= end) {
      result.push_back(entry);
    }
  }
  return result;
}

size_t AuditLogger::GetTotalEntryCount() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return entries_.size();
}

size_t AuditLogger::GetFailedLoginCount() const {
  std::lock_guard<std::mutex> lock(mutex_);

  size_t count = 0;
  for (const auto& entry : entries_) {
    if (entry.event_type == EventType::kLoginFailed) {
      ++count;
    }
  }
  return count;
}

size_t AuditLogger::GetUnauthorizedAccessCount() const {
  std::lock_guard<std::mutex> lock(mutex_);

  size_t count = 0;
  for (const auto& entry : entries_) {
    if (entry.event_type == EventType::kUnauthorizedAccess) {
      ++count;
    }
  }
  return count;
}

void AuditLogger::Flush() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (log_file_.is_open()) {
    log_file_.flush();
  }
}

void AuditLogger::Rotate() {
  std::lock_guard<std::mutex> lock(mutex_);

  // Check file size
  if (fs::exists(log_file_path_)) {
    auto file_size = fs::file_size(log_file_path_);
    if (file_size < kMaxFileSize) {
      return; // No need to rotate
    }
  }

  // Close current file
  if (log_file_.is_open()) {
    log_file_.close();
  }

  // Rename old file
  auto timestamp = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(timestamp);
  std::ostringstream oss;
  oss << log_file_path_ << "." << std::put_time(std::localtime(&time_t), "%Y%m%d%H%M%S");
  fs::rename(log_file_path_, oss.str());

  // Open new file
  log_file_.open(log_file_path_, std::ios::app);
}

std::string AuditLogger::EventTypeToString(EventType type) const {
  switch (type) {
  case EventType::kLogin:
    return "LOGIN";
  case EventType::kLogout:
    return "LOGOUT";
  case EventType::kLoginFailed:
    return "LOGIN_FAILED";
  case EventType::kPut:
    return "PUT";
  case EventType::kGet:
    return "GET";
  case EventType::kDelete:
    return "DELETE";
  case EventType::kBatchOperation:
    return "BATCH";
  case EventType::kExport:
    return "EXPORT";
  case EventType::kClearDatabase:
    return "CLEAR_DB";
  case EventType::kConfigChange:
    return "CONFIG";
  case EventType::kUserCreated:
    return "USER_CREATED";
  case EventType::kUserDeactivated:
    return "USER_DEACTIVATED";
  case EventType::kSessionExpired:
    return "SESSION_EXPIRED";
  case EventType::kUnauthorizedAccess:
    return "UNAUTHORIZED";
  default:
    return "UNKNOWN";
  }
}

void AuditLogger::WriteToFile(const AuditEntry& entry) {
  if (!log_file_.is_open()) {
    return;
  }

  log_file_ << "[" << TimeToString(entry.timestamp) << "] " << EventTypeToString(entry.event_type)
            << " " << (entry.success ? "OK" : "FAIL") << " "
            << "User:" << entry.username << " "
            << "IP:" << entry.ip_address << " " << entry.details << std::endl;
}

void AuditLogger::LoadExistingEntries() {
  // For simplicity, we're not loading existing entries on startup
  // In production, you might want to load the last N entries for quick querying
}

} // namespace audit
} // namespace core_engine
