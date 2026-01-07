#include <core_engine/common/logger.hpp>
#include <core_engine/engine.hpp>

#include <httplib.h>

#include <mutex>
#include <sstream>
#include <string>

// Modern, functional database web interface
static const char* kIndexHtml = R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width, initial-scale=1"/>
  <title>LSM Database Engine - Web Interface</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { 
      font-family: system-ui, -apple-system, sans-serif; 
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      min-height: 100vh;
      padding: 20px;
    }
    .container {
      max-width: 1400px;
      margin: 0 auto;
      background: white;
      border-radius: 16px;
      box-shadow: 0 20px 60px rgba(0,0,0,0.3);
      overflow: hidden;
    }
    .header {
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      padding: 40px;
      text-align: center;
    }
    .header h1 { font-size: 36px; margin-bottom: 10px; }
    .header p { opacity: 0.9; font-size: 15px; }
    .main { display: flex; flex-wrap: wrap; }
    .panel { flex: 1; min-width: 450px; padding: 25px; border-right: 1px solid #e0e0e0; }
    .panel:last-child { border-right: none; }
    .panel h2 { 
      font-size: 20px; 
      color: #667eea; 
      margin-bottom: 20px;
      padding-bottom: 10px;
      border-bottom: 2px solid #f0f0f0;
    }
    .form-group { margin-bottom: 15px; }
    label { 
      display: block; 
      font-weight: 600; 
      margin-bottom: 6px; 
      color: #555;
      font-size: 13px;
      text-transform: uppercase;
      letter-spacing: 0.5px;
    }
    input, textarea { 
      width: 100%;
      padding: 12px;
      border: 2px solid #e0e0e0;
      border-radius: 8px;
      font-size: 14px;
      font-family: 'Courier New', monospace;
      transition: all 0.2s;
    }
    input:focus, textarea:focus {
      outline: none;
      border-color: #667eea;
      box-shadow: 0 0 0 3px rgba(102, 126, 234, 0.1);
    }
    textarea { resize: vertical; min-height: 100px; }
    button {
      padding: 12px 24px;
      border: none;
      border-radius: 8px;
      font-size: 14px;
      font-weight: 600;
      cursor: pointer;
      transition: all 0.2s;
      margin-right: 8px;
      margin-bottom: 8px;
    }
    .btn-primary { background: #667eea; color: white; }
    .btn-primary:hover { background: #5568d3; transform: translateY(-1px); box-shadow: 0 4px 12px rgba(102, 126, 234, 0.4); }
    .btn-secondary { background: #6c757d; color: white; }
    .btn-secondary:hover { background: #5a6268; }
    .btn-danger { background: #dc3545; color: white; }
    .btn-danger:hover { background: #c82333; }
    .btn-success { background: #28a745; color: white; }
    .btn-success:hover { background: #218838; }
    .stats-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
      gap: 15px;
      margin-bottom: 20px;
    }
    .stat-card {
      background: linear-gradient(135deg, #f8f9fa 0%, #e9ecef 100%);
      padding: 15px;
      border-radius: 8px;
      border-left: 4px solid #667eea;
    }
    .stat-label { font-size: 11px; color: #666; font-weight: 600; text-transform: uppercase; }
    .stat-value { font-size: 24px; color: #333; font-weight: 700; margin-top: 5px; }
    .console {
      background: #1e1e1e;
      color: #d4d4d4;
      padding: 15px;
      border-radius: 8px;
      font-family: 'Courier New', monospace;
      font-size: 13px;
      max-height: 400px;
      overflow-y: auto;
      margin-top: 15px;
      line-height: 1.6;
    }
    .console .success { color: #4ec9b0; }
    .console .error { color: #f48771; }
    .console .info { color: #569cd6; }
    .entries-list {
      max-height: 400px;
      overflow-y: auto;
      border: 2px solid #e0e0e0;
      border-radius: 8px;
      margin-top: 15px;
    }
    .entry-item {
      padding: 12px 15px;
      border-bottom: 1px solid #f0f0f0;
      font-family: 'Courier New', monospace;
      font-size: 13px;
    }
    .entry-item:last-child { border-bottom: none; }
    .entry-key { color: #667eea; font-weight: 600; }
    .entry-value { color: #333; margin-left: 10px; }
    .empty-state {
      text-align: center;
      padding: 40px;
      color: #999;
      font-style: italic;
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <h1>🗄️ LSM Database Engine</h1>
      <p>Write-Ahead Log • MemTable • SSTables • Bloom Filters • Multi-Level Compaction</p>
    </div>
    
    <div class="main">
      <div class="panel">
        <h2>⚡ Operations</h2>
        
        <div class="form-group">
          <label>Key</label>
          <input type="text" id="key" placeholder="user_123" autocomplete="off"/>
        </div>
        
        <div class="form-group">
          <label>Value</label>
          <textarea id="value" placeholder='{"name":"Alice","age":30}'></textarea>
        </div>
        
        <div>
          <button class="btn-primary" onclick="doPut()">PUT</button>
          <button class="btn-success" onclick="doGet()">GET</button>
          <button class="btn-danger" onclick="doDelete()">DELETE</button>
        </div>
        
        <h2 style="margin-top: 30px;">📦 Bulk Operations</h2>
        
        <div class="form-group">
          <label>Batch Insert (key=value per line)</label>
          <textarea id="batch-input" placeholder="user_1=Alice&#10;user_2=Bob&#10;user_3=Carol"></textarea>
        </div>
        <button class="btn-primary" onclick="doBatchPut()">Batch Insert</button>
        
        <div style="margin-top: 20px;">
          <label>Generate Test Data</label>
          <div style="display: flex; gap: 10px; margin-top: 10px;">
            <input type="text" id="bulk-prefix" placeholder="prefix" value="test" style="flex: 1;"/>
            <input type="number" id="bulk-count" placeholder="count" value="100" style="flex: 1;"/>
            <button class="btn-secondary" onclick="doBulkInsert()">Generate</button>
          </div>
        </div>
      </div>
      
      <div class="panel">
        <h2>📊 Database Statistics</h2>
        <div class="stats-grid">
          <div class="stat-card">
            <div class="stat-label">MemTable Size</div>
            <div class="stat-value" id="stat-memtable-size">0 KB</div>
          </div>
          <div class="stat-card">
            <div class="stat-label">Entries</div>
            <div class="stat-value" id="stat-entries">0</div>
          </div>
          <div class="stat-card">
            <div class="stat-label">SSTables</div>
            <div class="stat-value" id="stat-sstables">0</div>
          </div>
          <div class="stat-card">
            <div class="stat-label">WAL Size</div>
            <div class="stat-value" id="stat-wal-size">0 KB</div>
          </div>
          <div class="stat-card">
            <div class="stat-label">Total Reads</div>
            <div class="stat-value" id="stat-reads">0</div>
          </div>
          <div class="stat-card">
            <div class="stat-label">Total Writes</div>
            <div class="stat-value" id="stat-writes">0</div>
          </div>
          <div class="stat-card">
            <div class="stat-label">Bloom Checks</div>
            <div class="stat-value" id="stat-bloom-checks">0</div>
          </div>
          <div class="stat-card">
            <div class="stat-label">Bloom Hit Rate</div>
            <div class="stat-value" id="stat-bloom-hitrate">0%</div>
          </div>
        </div>
        
        <h2>📋 All Entries</h2>
        <button class="btn-secondary" onclick="refreshEntries()">Refresh</button>
        <button class="btn-danger" onclick="clearDatabase()">Clear Database</button>
        <div id="entries-list" class="entries-list">
          <div class="empty-state">No entries yet</div>
        </div>
      </div>
    </div>
    
    <div class="panel" style="border-right: none;">
      <h2>💻 Console</h2>
      <button class="btn-secondary" onclick="clearConsole()">Clear</button>
      <div id="console" class="console">Ready.\n</div>
    </div>
  </div>

  <script>
    const keyEl = document.getElementById('key');
    const valueEl = document.getElementById('value');
    const consoleEl = document.getElementById('console');

    function log(msg, type = 'info') {
      const timestamp = new Date().toLocaleTimeString();
      consoleEl.innerHTML += `<span class="${type}">[${timestamp}] ${escapeHtml(msg)}</span>\n`;
      consoleEl.scrollTop = consoleEl.scrollHeight;
    }

    function escapeHtml(text) {
      const div = document.createElement('div');
      div.textContent = text;
      return div.innerHTML;
    }

    function formatBytes(bytes) {
      if (bytes < 1024) return bytes + ' B';
      if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
      return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
    }

    async function refreshStats() {
      try {
        const res = await fetch('/api/stats');
        const stats = await res.json();
        
        document.getElementById('stat-memtable-size').textContent = formatBytes(stats.memtable_size_bytes);
        document.getElementById('stat-entries').textContent = stats.memtable_entry_count;
        document.getElementById('stat-sstables').textContent = stats.sstable_count;
        document.getElementById('stat-wal-size').textContent = formatBytes(stats.wal_size_bytes);
        document.getElementById('stat-reads').textContent = stats.total_gets;
        document.getElementById('stat-writes').textContent = stats.total_puts;
        document.getElementById('stat-bloom-checks').textContent = stats.bloom_checks;
        
        const hitRate = stats.bloom_checks > 0 
          ? ((stats.bloom_hits / stats.bloom_checks) * 100).toFixed(1)
          : 0;
        document.getElementById('stat-bloom-hitrate').textContent = hitRate + '%';
      } catch (err) {
        log('Failed to refresh stats: ' + err.message, 'error');
      }
    }

    async function refreshEntries() {
      try {
        const res = await fetch('/api/entries');
        const data = await res.json();
        const listEl = document.getElementById('entries-list');
        
        if (data.entries.length === 0) {
          listEl.innerHTML = '<div class="empty-state">No entries</div>';
          return;
        }
        
        listEl.innerHTML = data.entries.map(e => 
          `<div class="entry-item"><span class="entry-key">${escapeHtml(e.key)}</span><span class="entry-value">= ${escapeHtml(e.value)}</span></div>`
        ).join('');
      } catch (err) {
        log('Failed to refresh entries: ' + err.message, 'error');
      }
    }

    async function doPut() {
      const key = keyEl.value.trim();
      const value = valueEl.value.trim();
      
      if (!key || !value) {
        log('Key and value required', 'error');
        return;
      }

      try {
        const res = await fetch('/api/put', {
          method: 'POST',
          headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
          body: new URLSearchParams({ key, value })
        });
        
        const text = await res.text();
        if (res.ok) {
          log(`✓ PUT "${key}"`, 'success');
          await refreshStats();
          await refreshEntries();
        } else {
          log(`✗ PUT failed: ${text}`, 'error');
        }
      } catch (err) {
        log('Error: ' + err.message, 'error');
      }
    }

    async function doGet() {
      const key = keyEl.value.trim();
      if (!key) {
        log('Key required', 'error');
        return;
      }

      try {
        const res = await fetch('/api/get?key=' + encodeURIComponent(key));
        const text = await res.text();
        
        if (res.ok) {
          log(`✓ GET "${key}" = "${text}"`, 'success');
          valueEl.value = text;
        } else if (res.status === 404) {
          log(`✗ Key "${key}" not found`, 'error');
        } else {
          log(`✗ GET failed: ${text}`, 'error');
        }
      } catch (err) {
        log('Error: ' + err.message, 'error');
      }
    }

    async function doDelete() {
      const key = keyEl.value.trim();
      if (!key) {
        log('Key required', 'error');
        return;
      }

      try {
        const res = await fetch('/api/delete', {
          method: 'POST',
          headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
          body: new URLSearchParams({ key })
        });
        
        const text = await res.text();
        if (res.ok) {
          log(`✓ DELETE "${key}"`, 'success');
          await refreshStats();
          await refreshEntries();
        } else {
          log(`✗ DELETE failed: ${text}`, 'error');
        }
      } catch (err) {
        log('Error: ' + err.message, 'error');
      }
    }

    async function doBatchPut() {
      const input = document.getElementById('batch-input').value.trim();
      if (!input) {
        log('Batch input empty', 'error');
        return;
      }

      const lines = input.split('\n').filter(l => l.trim());
      log(`Batch inserting ${lines.length} entries...`);
      
      let success = 0;
      for (const line of lines) {
        const [key, ...rest] = line.split('=');
        const value = rest.join('=');
        
        if (!key || !value) continue;
        
        try {
          const res = await fetch('/api/put', {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body: new URLSearchParams({ key: key.trim(), value: value.trim() })
          });
          if (res.ok) success++;
        } catch (err) {
          log(`Failed: ${key}`, 'error');
        }
      }
      
      log(`✓ Batch complete: ${success}/${lines.length}`, 'success');
      await refreshStats();
      await refreshEntries();
    }

    async function doBulkInsert() {
      const prefix = document.getElementById('bulk-prefix').value.trim() || 'key';
      const count = parseInt(document.getElementById('bulk-count').value) || 100;
      
      log(`Generating ${count} entries with prefix "${prefix}"...`);
      
      const startTime = Date.now();
      let success = 0;
      
      for (let i = 0; i < count; i++) {
        const key = `${prefix}_${i}`;
        const value = `value_${i}_${Date.now()}`;
        
        try {
          const res = await fetch('/api/put', {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body: new URLSearchParams({ key, value })
          });
          if (res.ok) success++;
          
          if ((i + 1) % 50 === 0) {
            log(`  Progress: ${i + 1}/${count}...`);
          }
        } catch (err) {
          // Continue on error
        }
      }
      
      const duration = ((Date.now() - startTime) / 1000).toFixed(2);
      log(`✓ Generated ${success}/${count} in ${duration}s`, 'success');
      await refreshStats();
      await refreshEntries();
    }

    async function clearDatabase() {
      if (!confirm('Delete all entries? This writes tombstones for all keys.')) return;
      
      try {
        const res = await fetch('/api/entries');
        const data = await res.json();
        
        log(`Deleting ${data.entries.length} entries...`);
        
        for (const entry of data.entries) {
          await fetch('/api/delete', {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body: new URLSearchParams({ key: entry.key })
          });
        }
        
        log('✓ Database cleared', 'success');
        await refreshStats();
        await refreshEntries();
      } catch (err) {
        log('Error clearing database: ' + err.message, 'error');
      }
    }

    function clearConsole() {
      consoleEl.innerHTML = 'Console cleared.\n';
    }

    refreshStats();
    refreshEntries();
    setInterval(refreshStats, 5000);
  </script>
</body>
</html>
)HTML";

int main(int argc, char** argv) {
  using core_engine::Engine;
  using core_engine::Log;
  using core_engine::LogLevel;

  const std::string db_dir = (argc >= 2) ? argv[1] : "./_web_demo";
  const int port = (argc >= 3) ? std::stoi(argv[2]) : 8080;

  Engine engine;
  auto status = engine.Open(db_dir);
  
  if (!status.ok()) {
    Log(LogLevel::kError, status.ToString());
    return 1;
  }

  std::mutex engine_mutex;
  httplib::Server server;

  server.Get("/", [&](const httplib::Request&, httplib::Response& res) {
    res.set_content(kIndexHtml, "text/html; charset=utf-8");
  });

  server.Get("/api/stats", [&](const httplib::Request&, httplib::Response& res) {
    std::lock_guard<std::mutex> lock(engine_mutex);
    const auto stats = engine.GetStats();
    
    std::ostringstream json;
    json << "{"
         << "\"memtable_size_bytes\":" << stats.memtable_size_bytes << ","
         << "\"memtable_entry_count\":" << stats.memtable_entry_count << ","
         << "\"sstable_count\":" << stats.sstable_count << ","
         << "\"wal_size_bytes\":" << stats.wal_size_bytes << ","
         << "\"avg_get_time_us\":" << stats.avg_get_time_us << ","
         << "\"avg_put_time_us\":" << stats.avg_put_time_us << ","
         << "\"total_gets\":" << stats.total_gets << ","
         << "\"total_puts\":" << stats.total_puts << ","
         << "\"bloom_checks\":" << stats.bloom_checks << ","
         << "\"bloom_hits\":" << stats.bloom_hits << ","
         << "\"bloom_false_positives\":" << stats.bloom_false_positives
         << "}";
    
    res.set_content(json.str(), "application/json");
  });

  server.Get("/api/entries", [&](const httplib::Request&, httplib::Response& res) {
    std::lock_guard<std::mutex> lock(engine_mutex);
    const auto entries = engine.GetAllEntries();
    
    auto escape_json = [](const std::string& s) -> std::string {
      std::string result;
      for (char c : s) {
        switch (c) {
          case '"': result += "\\\""; break;
          case '\\': result += "\\\\"; break;
          case '\n': result += "\\n"; break;
          case '\r': result += "\\r"; break;
          case '\t': result += "\\t"; break;
          default: result += c;
        }
      }
      return result;
    };
    
    std::ostringstream json;
    json << "{\"entries\":[";
    
    bool first = true;
    for (const auto& [key, value] : entries) {
      if (!first) json << ",";
      first = false;
      json << "{\"key\":\"" << escape_json(key) << "\","
           << "\"value\":\"" << escape_json(value) << "\"}";
    }
    
    json << "]}";
    res.set_content(json.str(), "application/json");
  });

  server.Post("/api/put", [&](const httplib::Request& req, httplib::Response& res) {
    if (!req.has_param("key") || !req.has_param("value")) {
      res.status = 400;
      res.set_content("Missing key or value", "text/plain");
      return;
    }

    const auto key = req.get_param_value("key");
    const auto value = req.get_param_value("value");

    std::lock_guard<std::mutex> lock(engine_mutex);
    const auto put_status = engine.Put(key, value);
    
    if (!put_status.ok()) {
      res.status = 500;
      res.set_content(put_status.ToString(), "text/plain");
      return;
    }

    res.set_content("OK", "text/plain");
  });

  server.Get("/api/get", [&](const httplib::Request& req, httplib::Response& res) {
    if (!req.has_param("key")) {
      res.status = 400;
      res.set_content("Missing key", "text/plain");
      return;
    }

    const auto key = req.get_param_value("key");

    std::lock_guard<std::mutex> lock(engine_mutex);
    const auto value = engine.Get(key);

    if (!value.has_value()) {
      res.status = 404;
      res.set_content("NOT_FOUND", "text/plain");
      return;
    }

    res.set_content(*value, "text/plain");
  });

  server.Post("/api/delete", [&](const httplib::Request& req, httplib::Response& res) {
    if (!req.has_param("key")) {
      res.status = 400;
      res.set_content("Missing key", "text/plain");
      return;
    }

    const auto key = req.get_param_value("key");

    std::lock_guard<std::mutex> lock(engine_mutex);
    const auto delete_status = engine.Delete(key);
    
    if (!delete_status.ok()) {
      res.status = 500;
      res.set_content(delete_status.ToString(), "text/plain");
      return;
    }

    res.set_content("OK", "text/plain");
  });

  Log(LogLevel::kInfo, "Web interface running");
  Log(LogLevel::kInfo, "Open http://127.0.0.1:" + std::to_string(port) + "/");
  Log(LogLevel::kInfo, "Database: " + db_dir);

  server.listen("127.0.0.1", port);
  return 0;
}
