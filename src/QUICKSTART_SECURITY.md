# Quick Start: LSM Database Engine v1.3 Security Features

This guide shows you how to use the new security features in 5 minutes.

## ✅ What You Get

- ✔️ **Authentication**: Secure user login with passwords
- ✔️ **Sessions**: 30-minute automatic timeout
- ✔️ **Roles**: Admin vs User permissions
- ✔️ **Audit Log**: Every action recorded
- ✔️ **Configuration**: Easy setup files

## 🚀 Quick Demo

### 1. Build Everything

```powershell
cd C:\Users\James\SystemProjects\LSMDatabaseEngine\src
cmake --build build\windows-vs2022-x64-debug --config Debug
```

### 2. Run Security Tests

```powershell
.\build\windows-vs2022-x64-debug\tests\Debug\security_tests.exe
```

You should see:
```
=== Security Tests ===
Test: Create User... PASSED
Test: Validate Credentials... PASSED
...
=== All Security Tests PASSED ===
```

### 3. Use Authentication in Code

```cpp
#include <core_engine/security/auth.hpp>
#include <core_engine/security/audit.hpp>

using namespace core_engine::security;
using namespace core_engine::audit;

int main() {
  // Create auth manager and audit logger
  AuthManager auth;
  AuditLogger audit("./audit.log");
  
  // Create a user
  auth.CreateUser("alice", "password123", {"user"});
  
  // Login
  if (auth.ValidateCredentials("alice", "password123")) {
    std::string session = auth.CreateSession("alice", "127.0.0.1");
    audit.LogLogin("alice", "127.0.0.1", true);
    
    // Use the session
    if (auth.ValidateSession(session)) {
      std::cout << "Alice is logged in!\n";
      
      // Check permissions
      if (auth.CanWrite("alice")) {
        std::cout << "Alice can write data\n";
        audit.LogPut("alice", "test_key", true);
      }
    }
    
    // Logout
    auth.InvalidateSession(session);
    audit.LogLogout("alice", "127.0.0.1");
  }
  
  return 0;
}
```

### 4. View Audit Log

The audit log is written to `./audit.log`:

```
[2026-01-06 10:30:15] LOGIN OK User:alice IP:127.0.0.1 User logged in
[2026-01-06 10:30:16] PUT OK User:alice IP:127.0.0.1 PUT key: test_key
[2026-01-06 10:30:17] LOGOUT OK User:alice IP:127.0.0.1 User logged out
```

### 5. Use Configuration

```cpp
#include <core_engine/config/app_config.hpp>

using namespace core_engine::config;

int main() {
  // Get configuration instance
  auto& config = AppConfig::Instance();
  
  // Load from file (if exists)
  config.Load("config.txt");
  
  // Access settings
  std::cout << "Port: " << config.Server().port << "\n";
  std::cout << "Auth required: " << config.Security().require_authentication << "\n";
  std::cout << "Data dir: " << config.Database().data_dir << "\n";
  
  // Save configuration
  config.MutableServer().port = 9090;
  config.Save("config.txt");
  
  return 0;
}
```

## 📋 Default Users

When you first run the system, these users are created:

- **Username**: `admin` / **Password**: `admin123` / **Roles**: admin, user
- **Username**: `user` / **Password**: `user123` / **Roles**: user

⚠️ **Change these passwords in production!**

## 🔒 Permission Checks

```cpp
AuthManager auth;

// Check specific role
if (auth.HasRole("admin", "admin")) {
  std::cout << "User is an admin\n";
}

// Check permissions
if (auth.CanWrite("user1")) {
  // Allow write
}

if (auth.IsAdmin("user1")) {
  // Allow admin actions
}
```

## 📊 Query Audit Log

```cpp
AuditLogger audit("./audit.log");

// Get recent 100 entries
auto recent = audit.GetRecentEntries(100);
for (const auto& entry : recent) {
  std::cout << entry.ToString() << "\n";
}

// Get entries for specific user
auto alice_entries = audit.GetEntriesByUser("alice");
std::cout << "Alice has " << alice_entries.size() << " log entries\n";

// Get failed logins
size_t failed = audit.GetFailedLoginCount();
std::cout << "Failed login attempts: " << failed << "\n";

// Get by event type
auto logins = audit.GetEntriesByType(EventType::kLogin);
std::cout << "Successful logins: " << logins.size() << "\n";
```

## 🎯 Common Use Cases

### Create New User

```cpp
AuthManager auth;
auth.CreateUser("newuser", "secure_password", {"user"});
```

### Login Flow

```cpp
std::string username = /* from form */;
std::string password = /* from form */;
std::string ip = /* from request */;

if (auth.ValidateCredentials(username, password)) {
  std::string session_id = auth.CreateSession(username, ip);
  // Store session_id in cookie
  audit.LogLogin(username, ip, true);
} else {
  audit.LogLogin(username, ip, false);
  // Show error
}
```

### Validate Request

```cpp
std::string session_id = /* from cookie */;

if (!auth.ValidateSession(session_id)) {
  // Redirect to login
  return;
}

auth.RefreshSession(session_id);  // Extend timeout
std::string username = auth.GetUsernameFromSession(session_id);

// Process request...
```

### Logout

```cpp
std::string session_id = /* from cookie */;
std::string username = auth.GetUsernameFromSession(session_id);
auth.InvalidateSession(session_id);
audit.LogLogout(username, /* ip */);
```

## 📁 Configuration File

Create `config.txt`:

```ini
# Server
server.host=127.0.0.1
server.port=8080

# Security
security.require_authentication=true
security.session_timeout_minutes=30
security.enable_audit_log=true
security.audit_log_path=./audit.log

# Database
database.data_dir=./_data
database.memtable_size_limit_mb=4
```

Load it:

```cpp
auto& config = AppConfig::Instance();
config.Load("config.txt");
```

## 🧪 Run All Tests

```powershell
# Build
cmake --build build\windows-vs2022-x64-debug --config Debug --target ALL_BUILD

# Run security tests
.\build\windows-vs2022-x64-debug\tests\Debug\security_tests.exe

# Run engine tests
.\build\windows-vs2022-x64-debug\tests\Debug\core_engine_tests.exe
```

## 🎉 You're Done!

You now have a fully secure database with:
- ✅ User authentication
- ✅ Session management
- ✅ Audit logging
- ✅ Configuration management
- ✅ Role-based access control

## 📖 Next Steps

- Read [MILESTONE_V1.3_SECURITY.md](./MILESTONE_V1.3_SECURITY.md) for full documentation
- Check [test_security.cpp](../tests/test_security.cpp) for more examples
- Deploy to production (change default passwords first!)

## ❓ Need Help?

- View test examples: `tests/test_security.cpp`
- Read full API reference: `MILESTONE_V1.3_SECURITY.md`
- Check header files: `include/core_engine/security/`

---

**Happy Secure Coding!** 🔐
