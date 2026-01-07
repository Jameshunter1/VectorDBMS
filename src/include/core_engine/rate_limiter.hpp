#pragma once

// core_engine/rate_limiter.hpp
//
// Purpose:
// - Token bucket rate limiter for API protection.
// - Prevents abuse and ensures fair resource allocation.
//
// v1.4: Advanced Features

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace core_engine {

/**
 * TOKEN BUCKET RATE LIMITER
 * 
 * Classic algorithm for rate limiting with burst capacity.
 * Each client gets a bucket that refills at a constant rate.
 * 
 * Parameters:
 * - rate: tokens per second (e.g., 100 = 100 requests/sec).
 * - burst: maximum tokens (allows short bursts above rate).
 * 
 * Example:
 *   RateLimiter limiter(100, 200);  // 100/sec, burst of 200
 *   if (limiter.Allow("user123")) {
 *     // Process request
 *   } else {
 *     // Return 429 Too Many Requests
 *   }
 * 
 * Benefits:
 * - Protects against DoS attacks.
 * - Ensures quality of service for all users.
 * - Prevents resource exhaustion.
 */
class RateLimiter {
 public:
  /**
   * Create a rate limiter.
   * 
   * @param rate Tokens per second (requests per second).
   * @param burst Maximum burst capacity (max tokens in bucket).
   */
  RateLimiter(double rate, double burst);
  
  /**
   * Check if a request is allowed for the given client.
   * 
   * @param client_id Unique identifier (IP, user ID, session ID).
   * @return true if request allowed, false if rate limit exceeded.
   */
  bool Allow(const std::string& client_id);
  
  /**
   * Get current token count for a client (for monitoring).
   * 
   * @param client_id Client identifier.
   * @return Current number of tokens (0.0 to burst).
   */
  double GetTokens(const std::string& client_id) const;
  
  /**
   * Reset rate limit for a client (admin operation).
   * 
   * @param client_id Client identifier.
   */
  void Reset(const std::string& client_id);
  
  /**
   * Get statistics for monitoring.
   */
  struct Stats {
    std::size_t total_clients;      // Number of tracked clients.
    std::size_t total_requests;     // Total requests processed.
    std::size_t allowed_requests;   // Requests that were allowed.
    std::size_t denied_requests;    // Requests that were denied.
    double allow_rate;              // Percentage of allowed requests.
  };
  Stats GetStats() const;
  
 private:
  struct Bucket {
    double tokens;                                      // Current token count.
    std::chrono::steady_clock::time_point last_update; // Last refill time.
  };
  
  const double rate_;   // Tokens per second.
  const double burst_;  // Maximum tokens.
  
  mutable std::mutex mutex_;
  std::unordered_map<std::string, Bucket> buckets_;
  
  // Statistics
  mutable std::size_t total_requests_ = 0;
  mutable std::size_t allowed_requests_ = 0;
  mutable std::size_t denied_requests_ = 0;
  
  // Refill tokens based on elapsed time.
  void Refill(Bucket& bucket);
};

/**
 * RATE LIMITER MIDDLEWARE
 * 
 * Wrapper for easy integration with web servers.
 * Provides configurable rate limits per endpoint.
 */
class RateLimiterMiddleware {
 public:
  RateLimiterMiddleware();
  
  /**
   * Configure rate limit for an endpoint.
   * 
   * @param endpoint Endpoint path (e.g., "/api/put").
   * @param rate Requests per second.
   * @param burst Maximum burst capacity.
   */
  void ConfigureEndpoint(const std::string& endpoint, double rate, double burst);
  
  /**
   * Check if request is allowed.
   * 
   * @param endpoint Endpoint being accessed.
   * @param client_id Client identifier.
   * @return true if allowed, false if rate limited.
   */
  bool AllowRequest(const std::string& endpoint, const std::string& client_id);
  
  /**
   * Get statistics for all endpoints.
   */
  std::unordered_map<std::string, RateLimiter::Stats> GetAllStats() const;
  
 private:
  mutable std::mutex mutex_;
  std::unordered_map<std::string, std::unique_ptr<RateLimiter>> limiters_;
  std::unique_ptr<RateLimiter> default_limiter_;  // Fallback for unconfigured endpoints.
};

}  // namespace core_engine
