#include <core_engine/rate_limiter.hpp>

// core_engine/rate_limiter.cpp
//
// Implementation of token bucket rate limiter.

#include <algorithm>

namespace core_engine {

RateLimiter::RateLimiter(double rate, double burst)
    : rate_(rate), burst_(burst) {}

bool RateLimiter::Allow(const std::string& client_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  total_requests_++;
  
  auto now = std::chrono::steady_clock::now();
  
  // Find or create bucket for this client.
  auto it = buckets_.find(client_id);
  if (it == buckets_.end()) {
    // New client: start with full burst capacity.
    buckets_[client_id] = Bucket{burst_, now};
    allowed_requests_++;
    return true;
  }
  
  auto& bucket = it->second;
  
  // Refill tokens based on elapsed time.
  auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(now - bucket.last_update);
  bucket.tokens = std::min(burst_, bucket.tokens + elapsed.count() * rate_);
  bucket.last_update = now;
  
  // Check if we have at least 1 token.
  if (bucket.tokens >= 1.0) {
    bucket.tokens -= 1.0;
    allowed_requests_++;
    return true;
  }
  
  // Rate limit exceeded.
  denied_requests_++;
  return false;
}

double RateLimiter::GetTokens(const std::string& client_id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  
  auto it = buckets_.find(client_id);
  if (it == buckets_.end()) {
    return burst_;  // New client has full capacity.
  }
  
  return it->second.tokens;
}

void RateLimiter::Reset(const std::string& client_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  auto it = buckets_.find(client_id);
  if (it != buckets_.end()) {
    it->second.tokens = burst_;
    it->second.last_update = std::chrono::steady_clock::now();
  }
}

RateLimiter::Stats RateLimiter::GetStats() const {
  std::lock_guard<std::mutex> lock(mutex_);
  
  Stats stats{};
  stats.total_clients = buckets_.size();
  stats.total_requests = total_requests_;
  stats.allowed_requests = allowed_requests_;
  stats.denied_requests = denied_requests_;
  stats.allow_rate = (total_requests_ > 0) 
    ? (static_cast<double>(allowed_requests_) / total_requests_ * 100.0) 
    : 100.0;
  
  return stats;
}

// ============================================================================
// RATE LIMITER MIDDLEWARE
// ============================================================================

RateLimiterMiddleware::RateLimiterMiddleware()
    : default_limiter_(std::make_unique<RateLimiter>(100.0, 200.0)) {  // Default: 100/sec with burst of 200.
}

void RateLimiterMiddleware::ConfigureEndpoint(const std::string& endpoint, double rate, double burst) {
  std::lock_guard<std::mutex> lock(mutex_);
  limiters_[endpoint] = std::make_unique<RateLimiter>(rate, burst);
}

bool RateLimiterMiddleware::AllowRequest(const std::string& endpoint, const std::string& client_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  auto it = limiters_.find(endpoint);
  if (it != limiters_.end() && it->second) {
    return it->second->Allow(client_id);
  }
  
  // Use default limiter for unconfigured endpoints.
  return default_limiter_->Allow(client_id);
}

std::unordered_map<std::string, RateLimiter::Stats> RateLimiterMiddleware::GetAllStats() const {
  std::lock_guard<std::mutex> lock(mutex_);
  
  std::unordered_map<std::string, RateLimiter::Stats> all_stats;
  
  for (const auto& [endpoint, limiter] : limiters_) {
    if (limiter) {
      all_stats[endpoint] = limiter->GetStats();
    }
  }
  
  all_stats["_default"] = default_limiter_->GetStats();
  
  return all_stats;
}

}  // namespace core_engine
