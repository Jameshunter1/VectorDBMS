#include <core_engine/metrics.hpp>

// core_engine/metrics.cpp
//
// Implementation of Prometheus metrics exporter.

#include <core_engine/engine.hpp>

#include <iomanip>
#include <sstream>

namespace core_engine {

// Map EngineStats to Engine::Stats for compatibility
struct MetricsCollector::EngineStats {
  std::size_t total_pages;
  std::size_t total_reads;
  std::size_t total_writes;
  std::size_t checksum_failures;
  std::size_t bloom_checks;          // Future feature
  std::size_t bloom_hits;            // Future feature
  std::size_t bloom_false_positives; // Future feature
  double avg_get_time_us;
  double avg_put_time_us;
  std::size_t total_gets;
  std::size_t total_puts;
};

}  // namespace core_engine

namespace core_engine {

// ============================================================================
// METRICS COLLECTOR IMPLEMENTATION
// ============================================================================

MetricsCollector::MetricsCollector() {
  // Initialize standard histogram buckets (latency in seconds).
  // Buckets: 0.001, 0.005, 0.01, 0.05, 0.1, 0.5, 1.0, 5.0, 10.0, +Inf
  std::vector<double> latency_buckets = {
    0.001, 0.005, 0.01, 0.05, 0.1, 0.5, 1.0, 5.0, 10.0, 
    std::numeric_limits<double>::infinity()
  };
  
  histograms_["core_engine_get_latency_seconds"] = {};
  histograms_["core_engine_put_latency_seconds"] = {};
  
  for (double bound : latency_buckets) {
    histograms_["core_engine_get_latency_seconds"].push_back({bound, 0});
    histograms_["core_engine_put_latency_seconds"].push_back({bound, 0});
  }
}

void MetricsCollector::IncrementCounter(const std::string& name, double value) {
  counters_[name].fetch_add(value, std::memory_order_relaxed);
}

void MetricsCollector::SetGauge(const std::string& name, double value) {
  std::lock_guard<std::mutex> lock(mutex_);
  gauges_[name] = value;
}

void MetricsCollector::ObserveHistogram(const std::string& name, double value) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  auto it = histograms_.find(name);
  if (it == histograms_.end()) {
    return;
  }
  
  // Find the bucket this value belongs to.
  for (auto& bucket : it->second) {
    if (value <= bucket.upper_bound) {
      bucket.count++;
      break;
    }
  }
}

std::string MetricsCollector::GetPrometheusText() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::ostringstream oss;
  
  // Timestamp
  auto now = std::chrono::system_clock::now();
  auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
    now.time_since_epoch()
  ).count();
  
  oss << "# Prometheus Metrics - Core Engine v1.4\n";
  oss << "# Generated: " << timestamp << "\n\n";
  
  // =========================================================================
  // COUNTERS
  // =========================================================================
  
  if (!counters_.empty()) {
    oss << "# COUNTERS\n";
    for (const auto& [name, value] : counters_) {
      double val = value.load(std::memory_order_relaxed);
      oss << "# HELP " << name << " Total count\n";
      oss << "# TYPE " << name << " counter\n";
      oss << name << " " << val << "\n\n";
    }
  }
  
  // =========================================================================
  // GAUGES
  // =========================================================================
  
  if (!gauges_.empty()) {
    oss << "# GAUGES\n";
    for (const auto& [name, value] : gauges_) {
      oss << "# HELP " << name << " Current value\n";
      oss << "# TYPE " << name << " gauge\n";
      oss << name << " " << value << "\n\n";
    }
  }
  
  // =========================================================================
  // HISTOGRAMS
  // =========================================================================
  
  if (!histograms_.empty()) {
    oss << "# HISTOGRAMS\n";
    for (const auto& [name, buckets] : histograms_) {
      oss << "# HELP " << name << " Latency distribution\n";
      oss << "# TYPE " << name << " histogram\n";
      
      std::uint64_t cumulative = 0;
      for (const auto& bucket : buckets) {
        cumulative += bucket.count;
        
        oss << name << "_bucket{le=\"";
        if (std::isinf(bucket.upper_bound)) {
          oss << "+Inf";
        } else {
          oss << bucket.upper_bound;
        }
        oss << "\"} " << cumulative << "\n";
      }
      
      oss << name << "_count " << cumulative << "\n";
      oss << name << "_sum " << cumulative << "\n\n";  // TODO: Track actual sum
    }
  }
  
  return oss.str();
}

void MetricsCollector::UpdateFromEngineStats(const EngineStats& stats) {
  // Update gauges for page I/O
  SetGauge("core_engine_total_pages", static_cast<double>(stats.total_pages));
  SetGauge("core_engine_total_reads", static_cast<double>(stats.total_reads));
  SetGauge("core_engine_total_writes", static_cast<double>(stats.total_writes));
  SetGauge("core_engine_checksum_failures", static_cast<double>(stats.checksum_failures));

  // Update counters
  IncrementCounter("core_engine_requests_total", 0);  // Ensure it exists
  IncrementCounter("core_engine_get_operations_total", 0);  // Will be updated by actual ops
  IncrementCounter("core_engine_put_operations_total", 0);

  // Performance metrics
  SetGauge("core_engine_avg_get_latency_microseconds", stats.avg_get_time_us);
  SetGauge("core_engine_avg_put_latency_microseconds", stats.avg_put_time_us);
  SetGauge("core_engine_total_get_operations", static_cast<double>(stats.total_gets));
  SetGauge("core_engine_total_put_operations", static_cast<double>(stats.total_puts));

  // Future: Bloom filter metrics (Year 2+)
  if (stats.bloom_checks > 0) {
    double effectiveness = static_cast<double>(stats.bloom_hits) / stats.bloom_checks * 100.0;
    SetGauge("core_engine_bloom_effectiveness_percent", effectiveness);
  }
}

// Helper to convert from Engine::Stats to EngineStats
void UpdateMetricsFromEngine(const Engine& engine) {
  auto stats = engine.GetStats();
  MetricsCollector::EngineStats es{stats.total_pages,
                                   stats.total_reads,
                                   stats.total_writes,
                                   stats.checksum_failures,
                                   0, // Placeholder: bloom_checks removed
                                   0, // Placeholder: bloom_hits removed
                                   0, // Placeholder: bloom_false_positives removed
                                   stats.avg_get_time_us,
                                   stats.avg_put_time_us,
                                   stats.total_gets,
                                   stats.total_puts};
  GetGlobalMetrics().UpdateFromEngineStats(es);
}

void MetricsCollector::Reset() {
  std::lock_guard<std::mutex> lock(mutex_);
  counters_.clear();
  gauges_.clear();
  for (auto& [name, buckets] : histograms_) {
    for (auto& bucket : buckets) {
      bucket.count = 0;
    }
  }
}

// ============================================================================
// GLOBAL METRICS INSTANCE
// ============================================================================

MetricsCollector& GetGlobalMetrics() {
  static MetricsCollector instance;
  return instance;
}

// ============================================================================
// SCOPED TIMER
// ============================================================================

ScopedTimer::ScopedTimer(std::string metric_name)
    : metric_name_(std::move(metric_name)),
      start_(std::chrono::steady_clock::now()) {}

ScopedTimer::~ScopedTimer() {
  auto end = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration<double>(end - start_).count();
  
  // Record latency in histogram
  GetGlobalMetrics().ObserveHistogram(metric_name_, duration);
}

// ============================================================================
// HEALTH CHECK
// ============================================================================

std::string HealthStatus::ToJson() const {
  std::ostringstream oss;
  oss << "{\n";
  oss << "  \"status\": \"";
  switch (status) {
    case Status::HEALTHY:   oss << "healthy"; break;
    case Status::DEGRADED:  oss << "degraded"; break;
    case Status::UNHEALTHY: oss << "unhealthy"; break;
  }
  oss << "\",\n";
  oss << "  \"message\": \"" << message << "\",\n";
  oss << "  \"components\": {\n";
  oss << "    \"database_open\": " << (database_open ? "true" : "false") << ",\n";
  oss << "    \"wal_healthy\": " << (wal_healthy ? "true" : "false") << ",\n";
  oss << "    \"memtable_healthy\": " << (memtable_healthy ? "true" : "false") << ",\n";
  oss << "    \"sstables_healthy\": " << (sstables_healthy ? "true" : "false") << "\n";
  oss << "  },\n";
  oss << "  \"resources\": {\n";
  oss << "    \"memory_usage_mb\": " << memory_usage_mb << ",\n";
  oss << "    \"disk_usage_mb\": " << disk_usage_mb << ",\n";
  oss << "    \"active_connections\": " << active_connections << "\n";
  oss << "  }\n";
  oss << "}\n";
  return oss.str();
}

HealthStatus CheckHealth(const Engine& engine) {
  HealthStatus health;
  
  auto stats = engine.GetStats();

  // Check if database is operational (page I/O working)
  health.database_open = true; // Database is open if we can get stats
  health.wal_healthy = true;                       // TODO: Add WAL health check (Year 1 Q4)
  health.memtable_healthy = true;                  // Deprecated (page-based architecture)
  health.sstables_healthy = true;                  // Deprecated (page-based architecture)

  // Calculate resource usage (page-based)
  health.memory_usage_mb = (stats.total_pages * 4096) / (1024.0 * 1024.0); // Buffer pool size
  health.disk_usage_mb = (stats.total_pages * 4096) / (1024.0 * 1024.0);   // Total pages on disk
  health.active_connections = 0;  // TODO: Track active connections
  
  // Determine overall status
  if (health.database_open && health.wal_healthy && 
      health.memtable_healthy && health.sstables_healthy) {
    health.status = HealthStatus::Status::HEALTHY;
    health.message = "All systems operational";
  } else if (health.database_open) {
    health.status = HealthStatus::Status::DEGRADED;
    health.message = "Some components degraded";
  } else {
    health.status = HealthStatus::Status::UNHEALTHY;
    health.message = "Database not operational";
  }
  
  return health;
}

}  // namespace core_engine
