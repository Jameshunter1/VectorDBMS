#include <core_engine/security/auth.hpp>

#include <iomanip>
#include <sstream>

// Simple password hashing (production would use bcrypt library)
// For now, using SHA-256 simulation with salt
namespace {
std::string SimpleHash(const std::string& input, const std::string& salt) {
  std::hash<std::string> hasher;
  auto hash = hasher(salt + input + salt);
  std::ostringstream oss;
  oss << std::hex << hash;
  return oss.str();
}
}  // namespace

namespace core_engine {
namespace security {

AuthManager::AuthManager() : rng_(std::random_device{}()) {
  // Create default admin user (admin/admin123)
  CreateUser("admin", "admin123", {"admin", "user"});
  // Create default user (user/user123)
  CreateUser("user", "user123", {"user"});
}

bool AuthManager::CreateUser(const std::string& username,
                              const std::string& password,
                              const std::vector<std::string>& roles) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  if (users_.count(username) > 0) {
    return false;  // User already exists
  }
  
  User user;
  user.username = username;
  user.password_hash = HashPassword(password);
  user.roles = roles;
  user.is_active = true;
  user.created_at = std::chrono::system_clock::now();
  
  users_[username] = user;
  return true;
}

bool AuthManager::ValidateCredentials(const std::string& username,
                                       const std::string& password) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  auto it = users_.find(username);
  if (it == users_.end()) {
    return false;
  }
  
  if (!it->second.is_active) {
    return false;
  }
  
  if (!VerifyPassword(password, it->second.password_hash)) {
    return false;
  }
  
  // Update last login
  it->second.last_login = std::chrono::system_clock::now();
  return true;
}

bool AuthManager::UserExists(const std::string& username) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return users_.count(username) > 0;
}

bool AuthManager::DeactivateUser(const std::string& username) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  auto it = users_.find(username);
  if (it == users_.end()) {
    return false;
  }
  
  it->second.is_active = false;
  return true;
}

std::string AuthManager::CreateSession(const std::string& username,
                                        const std::string& ip_address) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  if (users_.count(username) == 0) {
    return "";
  }
  
  Session session;
  session.session_id = GenerateSessionId();
  session.username = username;
  session.created_at = std::chrono::system_clock::now();
  session.last_activity = session.created_at;
  session.ip_address = ip_address;
  session.is_valid = true;
  
  sessions_[session.session_id] = session;
  return session.session_id;
}

bool AuthManager::ValidateSession(const std::string& session_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  auto it = sessions_.find(session_id);
  if (it == sessions_.end()) {
    return false;
  }
  
  if (!it->second.is_valid) {
    return false;
  }
  
  if (IsSessionExpired(it->second)) {
    it->second.is_valid = false;
    return false;
  }
  
  return true;
}

void AuthManager::InvalidateSession(const std::string& session_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  auto it = sessions_.find(session_id);
  if (it != sessions_.end()) {
    it->second.is_valid = false;
  }
}

void AuthManager::RefreshSession(const std::string& session_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  auto it = sessions_.find(session_id);
  if (it != sessions_.end() && it->second.is_valid) {
    it->second.last_activity = std::chrono::system_clock::now();
  }
}

std::string AuthManager::GetUsernameFromSession(const std::string& session_id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  
  auto it = sessions_.find(session_id);
  if (it != sessions_.end() && it->second.is_valid) {
    return it->second.username;
  }
  return "";
}

bool AuthManager::HasRole(const std::string& username, const std::string& role) const {
  std::lock_guard<std::mutex> lock(mutex_);
  
  auto it = users_.find(username);
  if (it == users_.end()) {
    return false;
  }
  
  const auto& roles = it->second.roles;
  return std::find(roles.begin(), roles.end(), role) != roles.end();
}

bool AuthManager::CanWrite(const std::string& username) const {
  return HasRole(username, "user") || HasRole(username, "admin");
}

bool AuthManager::CanRead(const std::string& username) const {
  return HasRole(username, "user") || HasRole(username, "admin");
}

bool AuthManager::CanDelete(const std::string& username) const {
  return HasRole(username, "admin");
}

bool AuthManager::IsAdmin(const std::string& username) const {
  return HasRole(username, "admin");
}

void AuthManager::CleanupExpiredSessions() {
  std::lock_guard<std::mutex> lock(mutex_);
  
  auto it = sessions_.begin();
  while (it != sessions_.end()) {
    if (IsSessionExpired(it->second)) {
      it->second.is_valid = false;
      it = sessions_.erase(it);
    } else {
      ++it;
    }
  }
}

size_t AuthManager::GetActiveSessionCount() const {
  std::lock_guard<std::mutex> lock(mutex_);
  
  size_t count = 0;
  for (const auto& [_, session] : sessions_) {
    if (session.is_valid && !IsSessionExpired(session)) {
      ++count;
    }
  }
  return count;
}

std::vector<std::string> AuthManager::GetActiveSessions() const {
  std::lock_guard<std::mutex> lock(mutex_);
  
  std::vector<std::string> active;
  for (const auto& [session_id, session] : sessions_) {
    if (session.is_valid && !IsSessionExpired(session)) {
      active.push_back(session_id);
    }
  }
  return active;
}

std::string AuthManager::HashPassword(const std::string& password) const {
  // In production, use bcrypt or Argon2
  // For demo, using simple hash with static salt
  return SimpleHash(password, "lsm_database_salt_v1");
}

bool AuthManager::VerifyPassword(const std::string& password,
                                  const std::string& hash) const {
  return HashPassword(password) == hash;
}

std::string AuthManager::GenerateSessionId() const {
  std::uniform_int_distribution<uint64_t> dist;
  std::ostringstream oss;
  oss << std::hex << dist(const_cast<std::mt19937&>(rng_))
      << dist(const_cast<std::mt19937&>(rng_));
  return oss.str();
}

bool AuthManager::IsSessionExpired(const Session& session) const {
  auto now = std::chrono::system_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(
      now - session.last_activity);
  return elapsed >= session.timeout;
}

}  // namespace security
}  // namespace core_engine
