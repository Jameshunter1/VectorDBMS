#pragma once

#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <vector>

namespace core_engine {
namespace security {

// User credentials and permissions
struct User {
  std::string username;
  std::string password_hash; // bcrypt hash
  std::vector<std::string> roles;
  bool is_active{true};
  std::chrono::system_clock::time_point created_at;
  std::chrono::system_clock::time_point last_login;
};

// Session information
struct Session {
  std::string session_id;
  std::string username;
  std::chrono::system_clock::time_point created_at;
  std::chrono::system_clock::time_point last_activity;
  std::chrono::minutes timeout{30}; // 30 minutes default
  std::string ip_address;
  bool is_valid{true};
};

// Authentication manager
class AuthManager {
public:
  AuthManager();
  ~AuthManager() = default;

  // User management
  bool CreateUser(const std::string& username, const std::string& password,
                  const std::vector<std::string>& roles = {"user"});
  bool ValidateCredentials(const std::string& username, const std::string& password);
  bool UserExists(const std::string& username) const;
  bool DeactivateUser(const std::string& username);

  // Session management
  std::string CreateSession(const std::string& username, const std::string& ip_address);
  bool ValidateSession(const std::string& session_id);
  void InvalidateSession(const std::string& session_id);
  void RefreshSession(const std::string& session_id);
  std::string GetUsernameFromSession(const std::string& session_id) const;

  // Permission checking
  bool HasRole(const std::string& username, const std::string& role) const;
  bool CanWrite(const std::string& username) const;
  bool CanRead(const std::string& username) const;
  bool CanDelete(const std::string& username) const;
  bool IsAdmin(const std::string& username) const;

  // Utility
  void CleanupExpiredSessions();
  size_t GetActiveSessionCount() const;
  std::vector<std::string> GetActiveSessions() const;

private:
  std::string HashPassword(const std::string& password) const;
  bool VerifyPassword(const std::string& password, const std::string& hash) const;
  std::string GenerateSessionId() const;
  bool IsSessionExpired(const Session& session) const;

  mutable std::mutex mutex_;
  std::map<std::string, User> users_;
  std::map<std::string, Session> sessions_;
  std::mt19937 rng_;
};

} // namespace security
} // namespace core_engine
