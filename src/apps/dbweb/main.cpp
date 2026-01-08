#include <core_engine/common/logger.hpp>
#include <core_engine/engine.hpp>
#include <core_engine/metrics.hpp>

#include <httplib.h>

#include <filesystem>
#include <mutex>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

// Enhanced web interface with advanced features
// Split into two parts to avoid MSVC 64KB string literal limit
static const char* kIndexHtml_Part1 = R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width, initial-scale=1"/>
  <title>Vectis Database Engine - Enhanced UI</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { 
      font-family: 'Segoe UI', system-ui, -apple-system, sans-serif; 
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      min-height: 100vh;
      padding: 20px;
    }
    .container {
      max-width: 1600px;
      margin: 0 auto;
      background: white;
      border-radius: 16px;
      box-shadow: 0 20px 60px rgba(0,0,0,0.3);
      overflow: hidden;
    }
    .header {
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      padding: 30px 40px;
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    .header h1 { font-size: 32px; }
    .header-stats { display: flex; gap: 30px; font-size: 14px; }
    .header-stat { text-align: center; }
    .header-stat-value { font-size: 24px; font-weight: 700; }
    .header-stat-label { opacity: 0.9; font-size: 11px; text-transform: uppercase; }
    
    .tabs {
      display: flex;
      background: #f8f9fa;
      border-bottom: 2px solid #e0e0e0;
    }
    .tab {
      padding: 15px 30px;
      cursor: pointer;
      border: none;
      background: none;
      font-size: 14px;
      font-weight: 600;
      color: #666;
      transition: all 0.2s;
      border-bottom: 3px solid transparent;
    }
    .tab:hover { color: #667eea; background: rgba(102, 126, 234, 0.05); }
    .tab.active { color: #667eea; border-bottom-color: #667eea; background: white; }
    
    .tab-content { display: none; padding: 30px; }
    .tab-content.active { display: block; }
    
    .grid-2 { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; }
    .grid-3 { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 15px; }
    
    .card {
      background: white;
      border: 1px solid #e0e0e0;
      border-radius: 8px;
      padding: 20px;
    }
    .card h3 { color: #667eea; margin-bottom: 15px; font-size: 16px; }
    
    .form-group { margin-bottom: 15px; }
    label { 
      display: block; 
      font-weight: 600; 
      margin-bottom: 6px; 
      color: #555;
      font-size: 12px;
      text-transform: uppercase;
    }
    input, textarea, select { 
      width: 100%;
      padding: 10px;
      border: 2px solid #e0e0e0;
      border-radius: 6px;
      font-size: 14px;
      font-family: 'Courier New', monospace;
      transition: all 0.2s;
    }
    input:focus, textarea:focus, select:focus {
      outline: none;
      border-color: #667eea;
      box-shadow: 0 0 0 3px rgba(102, 126, 234, 0.1);
    }
    textarea { resize: vertical; min-height: 80px; }
    
    button {
      padding: 10px 20px;
      border: none;
      border-radius: 6px;
      font-size: 13px;
      font-weight: 600;
      cursor: pointer;
      transition: all 0.2s;
      margin-right: 8px;
      margin-bottom: 8px;
    }
    .btn-primary { background: #667eea; color: white; }
    .btn-primary:hover { background: #5568d3; transform: translateY(-1px); }
    .btn-success { background: #28a745; color: white; }
    .btn-success:hover { background: #218838; }
    .btn-danger { background: #dc3545; color: white; }
    .btn-danger:hover { background: #c82333; }
    .btn-secondary { background: #6c757d; color: white; }
    .btn-secondary:hover { background: #5a6268; }
    .btn-small { padding: 6px 12px; font-size: 11px; }
    
    .stat-card {
      background: linear-gradient(135deg, #f8f9fa 0%, #e9ecef 100%);
      padding: 15px;
      border-radius: 8px;
      border-left: 4px solid #667eea;
      text-align: center;
    }
    .stat-label { font-size: 11px; color: #666; font-weight: 600; text-transform: uppercase; }
    .stat-value { font-size: 28px; color: #333; font-weight: 700; margin-top: 5px; }
    
    .search-box {
      display: flex;
      gap: 10px;
      margin-bottom: 15px;
    }
    .search-box input { flex: 1; }
    
    .entries-table {
      width: 100%;
      border-collapse: collapse;
      margin-top: 15px;
    }
    .entries-table th {
      background: #f8f9fa;
      padding: 12px;
      text-align: left;
      font-size: 12px;
      text-transform: uppercase;
      color: #666;
      border-bottom: 2px solid #e0e0e0;
    }
    .entries-table td {
      padding: 10px 12px;
      border-bottom: 1px solid #f0f0f0;
      font-family: 'Courier New', monospace;
      font-size: 13px;
    }
    .entries-table tr:hover { background: #f8f9fa; }
    .entry-key { color: #667eea; font-weight: 600; }
    .entry-value { color: #333; }
    .entry-actions { display: flex; gap: 5px; }
    
    .pagination {
      display: flex;
      justify-content: center;
      align-items: center;
      gap: 10px;
      margin-top: 20px;
      padding: 15px;
      background: #f8f9fa;
      border-radius: 8px;
    }
    .page-btn { 
      padding: 8px 12px;
      background: white;
      border: 1px solid #ddd;
      border-radius: 4px;
      cursor: pointer;
      font-size: 12px;
    }
    .page-btn:hover { background: #667eea; color: white; border-color: #667eea; }
    .page-btn.active { background: #667eea; color: white; border-color: #667eea; }
    .page-btn:disabled { opacity: 0.5; cursor: not-allowed; }
    
    .file-tree {
      background: #f8f9fa;
      border-radius: 8px;
      padding: 15px;
      max-height: 500px;
      overflow-y: auto;
      font-family: 'Courier New', monospace;
      font-size: 13px;
    }
    .file-item { padding: 8px; border-bottom: 1px solid #e0e0e0; display: flex; justify-content: space-between; }
    .file-item:hover { background: white; }
    .file-name { color: #333; }
    .file-size { color: #999; font-size: 11px; }
    .folder { color: #667eea; font-weight: 600; cursor: pointer; }
    
    .console {
      background: #1e1e1e;
      color: #d4d4d4;
      padding: 15px;
      border-radius: 8px;
      font-family: 'Courier New', monospace;
      font-size: 12px;
      max-height: 400px;
      overflow-y: auto;
      line-height: 1.6;
    }
    .console .success { color: #4ec9b0; }
    .console .error { color: #f48771; }
    .console .info { color: #569cd6; }
    
    .empty-state {
      text-align: center;
      padding: 60px 20px;
      color: #999;
      font-style: italic;
    }
    
    .level-badge {
      display: inline-block;
      padding: 4px 8px;
      border-radius: 4px;
      font-size: 11px;
      font-weight: 600;
      margin-right: 5px;
    }
    .level-0 { background: #ff6b6b; color: white; }
    .level-1 { background: #ffa500; color: white; }
    .level-2 { background: #4ecdc4; color: white; }
    
    .progress-bar {
      width: 100%;
      height: 8px;
      background: #e0e0e0;
      border-radius: 4px;
      overflow: hidden;
      margin-top: 8px;
    }
    .progress-fill {
      height: 100%;
      background: linear-gradient(90deg, #667eea, #764ba2);
      transition: width 0.3s;
    }
    
    .export-section { display: flex; gap: 10px; margin-top: 15px; }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <div>
        <h1>🗄️ Vectis Database Engine</h1>
        <p style="opacity: 0.9; font-size: 13px; margin-top: 5px;">Enhanced Management Interface</p>
      </div>
      <div class="header-stats">
        <div class="header-stat">
          <div class="header-stat-value" id="header-entries">0</div>
          <div class="header-stat-label">Entries</div>
        </div>
        <div class="header-stat">
          <div class="header-stat-value" id="header-pages">0</div>
          <div class="header-stat-label">Pages</div>
        </div>
        <div class="header-stat">
          <div class="header-stat-value" id="header-ops">0</div>
          <div class="header-stat-label">Get/Put Ops</div>
        </div>
      </div>
    </div>
    
    <div class="tabs">
      <button class="tab active" data-tab="operations" onclick="switchTab('operations', this)">⚡ Operations</button>
      <button class="tab" data-tab="vector" onclick="switchTab('vector', this)">🔍 Vector Search</button>
      <button class="tab" data-tab="browse" onclick="switchTab('browse', this)">📋 Browse Data</button>
      <button class="tab" data-tab="stats" onclick="switchTab('stats', this)">📊 Statistics</button>
      <button class="tab" data-tab="files" onclick="switchTab('files', this)">📁 Files</button>
      <button class="tab" data-tab="console" onclick="switchTab('console', this)">💻 Console</button>
    </div>
)HTML";

// Part 1b: Tab contents start
static const char* kIndexHtml_Part1b = R"HTML(    
    <div id="tab-operations" class="tab-content active">
      <div class="grid-2">
        <div class="card">
          <h3>Single Operations</h3>
          <div class="form-group">
            <label>Key</label>
            <input type="text" id="key" placeholder="user_123" autocomplete="off"/>
          </div>
          <div class="form-group">
            <label>Value</label>
            <textarea id="value" placeholder='{"name":"Alice","age":30}'></textarea>
          </div>
          <button class="btn-primary" onclick="doPut()">PUT</button>
          <button class="btn-success" onclick="doGet()">GET</button>
          <button class="btn-danger" onclick="doDelete()">DELETE</button>
        </div>
        
        <div class="card">
          <h3>Bulk Operations</h3>
          <div class="form-group">
            <label>Batch Insert (key=value per line)</label>
            <textarea id="batch-input" placeholder="user_1=Alice&#10;user_2=Bob"></textarea>
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
      </div>
    </div>
    
    <div id="tab-vector" class="tab-content">
      <div class="grid-2">
        <div class="card">
          <h3>Insert Vector</h3>
          <div class="form-group">
            <label>Key</label>
            <input type="text" id="vector-key" placeholder="doc:example_001" autocomplete="off"/>
          </div>
          <div class="form-group">
            <label>Vector (comma-separated floats)</label>
            <textarea id="vector-data" placeholder="0.1,0.2,0.3,0.4,0.5,..." rows="4"></textarea>
          </div>
          <button class="btn-primary" onclick="doPutVector()">Insert Vector</button>
          <button class="btn-success" onclick="doGetVector()">Get Vector</button>
          <p style="margin-top: 10px; font-size: 12px; color: #666;">
            Vectors must match the configured dimension to pass validation.
          </p>
          <div style="margin-top: 15px;">
            <button class="btn-secondary btn-small" id="vector-random-btn" onclick="generateRandomVector()">
              Generate Random (<span id="random-dim-label">128</span>-dim)
            </button>
            <div style="margin-top: 8px; font-size: 12px; color: #666;">
              Configured dimension: <span id="configured-dimension">128</span>
            </div>
          </div>
        </div>
        
        <div class="card">
          <h3>Similarity Search</h3>
          <div class="form-group">
            <label>Query Vector (comma-separated floats)</label>
            <textarea id="query-vector" placeholder="0.1,0.2,0.3,0.4,0.5,..." rows="4"></textarea>
          </div>
          <div class="form-group">
            <label>Number of Results (k)</label>
            <input type="number" id="search-k" value="5" min="1" max="100"/>
          </div>
          <button class="btn-primary" onclick="doVectorSearch()">Search Similar</button>
          <button class="btn-secondary" onclick="copyVectorToQuery()">Copy Insert Vector to Query</button>
          
          <div id="search-results" style="margin-top: 20px;"></div>
        </div>
      </div>
      
      <div class="card" style="margin-top: 20px;">
        <h3>Vector Index Statistics</h3>
        <div class="grid-3">
          <div class="stat-card">
            <div class="stat-label">Index Enabled</div>
            <div class="stat-value" id="vector-enabled">-</div>
          </div>
          <div class="stat-card">
            <div class="stat-label">Total Vectors</div>
            <div class="stat-value" id="vector-count">0</div>
          </div>
          <div class="stat-card">
            <div class="stat-label">Dimension</div>
            <div class="stat-value" id="vector-dimension">0</div>
          </div>
          <div class="stat-card">
            <div class="stat-label">Distance Metric</div>
            <div class="stat-value" id="vector-metric">-</div>
          </div>
          <div class="stat-card">
            <div class="stat-label">HNSW Layers</div>
            <div class="stat-value" id="vector-layers">0</div>
          </div>
          <div class="stat-card">
            <div class="stat-label">Avg Connections</div>
            <div class="stat-value" id="vector-connections">0</div>
          </div>
        </div>
        <button class="btn-secondary btn-small" onclick="refreshVectorStats()" style="margin-top: 15px;">🔄 Refresh Stats</button>
      </div>

      <div class="card" style="margin-top: 20px;">
        <h3>Bulk Vector Loader</h3>
        <div class="form-group">
          <label>Key Prefix</label>
          <input type="text" id="bulk-vector-prefix" value="vector" placeholder="vector"/>
        </div>
        <div class="form-group">
          <label>Vector Count</label>
          <input type="number" id="bulk-vector-count" value="25" min="1" max="1000"/>
        </div>
        <div class="form-group">
          <label>Value Range (-1 to 1)</label>
          <input type="number" id="bulk-vector-range" value="1" min="0.1" max="10" step="0.1" />
        </div>
        <button class="btn-primary" onclick="doBulkVectorInsert()">Generate & Insert</button>
        <p style="margin-top: 10px; font-size: 12px; color: #666;">
          Random vectors respect the configured dimension and automatically appear in the Browse Data tab.
        </p>
      </div>
    </div>
    
    <div id="tab-browse" class="tab-content">
      <div class="card">
        <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 15px;">
          <h3>Database Entries (<span id="total-entries">0</span> total)</h3>
          <div>
            <button class="btn-secondary btn-small" onclick="refreshBrowse()">🔄 Refresh</button>
            <button class="btn-success btn-small" onclick="exportData()">📥 Export JSON</button>
            <button class="btn-danger btn-small" onclick="clearDatabase()">🗑️ Clear All</button>
          </div>
        </div>
        
        <div class="search-box">
          <input type="text" id="search-key" placeholder="Search keys..." oninput="filterEntries()"/>
          <select id="sort-order" onchange="sortEntries()">
            <option value="asc">Sort A → Z</option>
            <option value="desc">Sort Z → A</option>
          </select>
        </div>
        
        <div style="max-height: 600px; overflow-y: auto;">
          <table class="entries-table" id="entries-table">
            <thead>
              <tr>
                <th style="width: 40%;">Key</th>
                <th style="width: 45%;">Value</th>
                <th style="width: 15%;">Actions</th>
              </tr>
            </thead>
            <tbody id="entries-tbody">
              <tr><td colspan="3" class="empty-state">No entries</td></tr>
            </tbody>
          </table>
        </div>
        
        <div class="pagination" id="pagination">
          <button class="page-btn" onclick="prevPage()" id="prev-btn">← Prev</button>
          <span id="page-info">Page 1 of 1</span>
          <button class="page-btn" onclick="nextPage()" id="next-btn">Next →</button>
          <select id="page-size" onchange="changePageSize()">
            <option value="10">10 per page</option>
            <option value="25" selected>25 per page</option>
            <option value="50">50 per page</option>
            <option value="100">100 per page</option>
          </select>
        </div>
      </div>
    </div>
    
    <div id="tab-stats" class="tab-content">
      <div class="grid-3">
        <div class="stat-card">
          <div class="stat-label">Total Pages</div>
          <div class="stat-value" id="stat-total-pages">0</div>
        </div>
        <div class="stat-card">
          <div class="stat-label">Disk Reads</div>
          <div class="stat-value" id="stat-disk-reads">0</div>
        </div>
        <div class="stat-card">
          <div class="stat-label">Disk Writes</div>
          <div class="stat-value" id="stat-disk-writes">0</div>
        </div>
        <div class="stat-card">
          <div class="stat-label">Checksum Failures</div>
          <div class="stat-value" id="stat-checksum-failures">0</div>
        </div>
        <div class="stat-card">
          <div class="stat-label">Total Entries</div>
          <div class="stat-value" id="stat-db-entries">0</div>
        </div>
        <div class="stat-card">
          <div class="stat-label">Get + Put Ops</div>
          <div class="stat-value" id="stat-total-ops">0</div>
        </div>
      </div>

      <div class="card" style="margin-top: 20px;">
        <h3>Latency & Throughput</h3>
        <div class="grid-3">
          <div class="stat-card">
            <div class="stat-label">Avg GET Time</div>
            <div class="stat-value" id="stat-avg-get">0 µs</div>
          </div>
          <div class="stat-card">
            <div class="stat-label">Avg PUT Time</div>
            <div class="stat-value" id="stat-avg-put">0 µs</div>
          </div>
          <div class="stat-card">
            <div class="stat-label">Total GETs</div>
            <div class="stat-value" id="stat-total-gets">0</div>
          </div>
          <div class="stat-card">
            <div class="stat-label">Total PUTs</div>
            <div class="stat-value" id="stat-total-puts">0</div>
          </div>
        </div>
      </div>
    </div>
)HTML";

// Part 2: Tabs and JavaScript start
static const char* kIndexHtml_Part2a = R"HTML(    
    <div id="tab-files" class="tab-content">
      <div class="card">
        <h3>Database Files</h3>
        <button class="btn-secondary btn-small" onclick="refreshFiles()">🔄 Refresh</button>
        <div class="file-tree" id="file-tree">Loading...</div>
      </div>
    </div>
    
    <div id="tab-console" class="tab-content">
      <div class="card">
        <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 15px;">
          <h3>Console Log</h3>
          <button class="btn-secondary btn-small" onclick="clearConsole()">Clear</button>
        </div>
        <div id="console" class="console">Ready.\n</div>
      </div>
    </div>
  </div>

  <script>
    // State
    let allEntries = [];
    let filteredEntries = [];
    let kvEntries = [];
    let vectorEntries = [];
    let currentPage = 1;
    let pageSize = 25;
    let configuredVectorDimension = 128;
    
    const keyEl = document.getElementById('key');
    const valueEl = document.getElementById('value');
    const consoleEl = document.getElementById('console');

    function switchTab(tabName, buttonEl = null) {
      document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
      document.querySelectorAll('.tab-content').forEach(t => t.classList.remove('active'));

      if (buttonEl) {
        buttonEl.classList.add('active');
      } else {
        const fallback = document.querySelector(`.tab[data-tab="${tabName}"]`);
        if (fallback) fallback.classList.add('active');
      }

      const target = document.getElementById('tab-' + tabName);
      if (target) target.classList.add('active');
      
      if (tabName === 'browse') refreshBrowse();
      if (tabName === 'stats') refreshStats();
      if (tabName === 'files') refreshFiles();
      if (tabName === 'vector') refreshVectorStats();
    }

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

    function mergeEntrySources() {
      allEntries = [...kvEntries, ...vectorEntries];
      document.getElementById('total-entries').textContent = allEntries.length;
      const hasSearch = document.getElementById('search-key').value.trim().length > 0;
      if (hasSearch) {
        filterEntries();
      } else {
        filteredEntries = [...allEntries];
        currentPage = 1;
        renderEntries();
      }
    }

    function upsertVectorEntry(key, vectorData) {
      const preview = vectorData.length > 120 ? vectorData.substring(0, 120) + '...' : vectorData;
      const entry = {
        key,
        value: `[vector] ${preview}`,
        entryType: 'vector',
        vectorRaw: vectorData
      };
      const idx = vectorEntries.findIndex(e => e.key === key);
      if (idx >= 0) {
        vectorEntries[idx] = entry;
      } else {
        vectorEntries.push(entry);
      }
      mergeEntrySources();
    }

    function removeVectorEntry(key) {
      vectorEntries = vectorEntries.filter(e => e.key !== key);
      mergeEntrySources();
    }

    async function refreshStats() {
      try {
        const res = await fetch('/api/stats');
        const stats = await res.json();
        
        document.getElementById('stat-total-pages').textContent = stats.total_pages;
        document.getElementById('stat-disk-reads').textContent = stats.total_reads;
        document.getElementById('stat-disk-writes').textContent = stats.total_writes;
        document.getElementById('stat-checksum-failures').textContent = stats.checksum_failures;
        document.getElementById('stat-db-entries').textContent = stats.total_entries;
        document.getElementById('stat-total-ops').textContent = stats.total_gets + stats.total_puts;
        document.getElementById('stat-avg-get').textContent = stats.avg_get_time_us.toFixed(2) + ' µs';
        document.getElementById('stat-avg-put').textContent = stats.avg_put_time_us.toFixed(2) + ' µs';
        document.getElementById('stat-total-gets').textContent = stats.total_gets;
        document.getElementById('stat-total-puts').textContent = stats.total_puts;

        document.getElementById('header-entries').textContent = stats.total_entries;
        document.getElementById('header-pages').textContent = stats.total_pages;
        document.getElementById('header-ops').textContent = stats.total_gets + stats.total_puts;
      } catch (err) {
        log('Failed to refresh stats: ' + err.message, 'error');
      }
    }

)HTML";

static const char* kIndexHtml_Part2b = R"HTML(

    async function refreshBrowse() {
      try {
        const [kvRes, vectorRes] = await Promise.all([
          fetch('/api/entries'),
          fetch('/api/vector/list')
        ]);

        if (!kvRes.ok) {
          throw new Error('Entries API returned ' + kvRes.status);
        }

        const kvData = await kvRes.json();
        kvEntries = kvData.entries.map(entry => ({ ...entry, entryType: 'kv' }));

        if (vectorRes.ok) {
          const vectorData = await vectorRes.json();
          vectorEntries = vectorData.vectors.map(entry => {
            const truncated = entry.vector.length > 120
              ? `${entry.vector.substring(0, 120)}...`
              : entry.vector;
            return {
              key: entry.key,
              value: `[vector dim=${entry.dimension}] ${truncated}`,
              entryType: 'vector',
              vectorRaw: entry.vector
            };
          });
        } else {
          vectorEntries = [];
        }

        mergeEntrySources();
      } catch (err) {
        log('Failed to load entries: ' + err.message, 'error');
      }
    }

    function filterEntries() {
      const search = document.getElementById('search-key').value.toLowerCase();
      if (!search) {
        filteredEntries = [...allEntries];
        currentPage = 1;
        renderEntries();
        return;
      }
      filteredEntries = allEntries.filter(e => {
        const valueText = typeof e.value === 'string' ? e.value : JSON.stringify(e.value ?? '');
        return e.key.toLowerCase().includes(search) || valueText.toLowerCase().includes(search);
      });
      currentPage = 1;
      renderEntries();
    }

    function sortEntries() {
      const order = document.getElementById('sort-order').value;
      filteredEntries.sort((a, b) => {
        return order === 'asc' ? a.key.localeCompare(b.key) : b.key.localeCompare(a.key);
      });
      renderEntries();
    }

    function renderEntries() {
      const tbody = document.getElementById('entries-tbody');
      
      if (filteredEntries.length === 0) {
        tbody.innerHTML = '<tr><td colspan="3" class="empty-state">No entries found</td></tr>';
        return;
      }
      
      const start = (currentPage - 1) * pageSize;
      const end = Math.min(start + pageSize, filteredEntries.length);
      const pageEntries = filteredEntries.slice(start, end);
      
      tbody.innerHTML = pageEntries.map(e => {
        const entryType = e.entryType || 'kv';
        const rawValue = typeof e.value === 'string' ? e.value : JSON.stringify(e.value ?? '');
        const displayValue = rawValue.length > 100 ? rawValue.substring(0, 100) + '...' : rawValue;
        const keyArg = JSON.stringify(e.key);
        const typeArg = JSON.stringify(entryType);
        const viewLabel = entryType === 'vector' ? 'Inspect' : 'View';
        const deleteLabel = entryType === 'vector' ? 'Remove' : 'Delete';
        const deleteClass = entryType === 'vector' ? 'btn-secondary' : 'btn-danger';
        return `
          <tr>
            <td class="entry-key">${escapeHtml(e.key)}</td>
            <td class="entry-value">${escapeHtml(displayValue)}</td>
            <td class="entry-actions">
              <button class="btn-success btn-small" onclick='viewEntry(${keyArg}, ${typeArg})'>${viewLabel}</button>
              <button class="${deleteClass} btn-small" onclick='deleteEntry(${keyArg}, ${typeArg})'>${deleteLabel}</button>
            </td>
          </tr>
        `;
      }).join('');
      
      // Update pagination
      const totalPages = Math.ceil(filteredEntries.length / pageSize);
      document.getElementById('page-info').textContent = `Page ${currentPage} of ${totalPages}`;
      document.getElementById('prev-btn').disabled = currentPage === 1;
      document.getElementById('next-btn').disabled = currentPage === totalPages;
    }

    function prevPage() {
      if (currentPage > 1) {
        currentPage--;
        renderEntries();
      }
    }

    function nextPage() {
      const totalPages = Math.ceil(filteredEntries.length / pageSize);
      if (currentPage < totalPages) {
        currentPage++;
        renderEntries();
      }
    }

    function changePageSize() {
      pageSize = parseInt(document.getElementById('page-size').value);
      currentPage = 1;
      renderEntries();
    }

    async function viewEntry(key, entryType = 'kv') {
      if (entryType === 'vector') {
        const entry = vectorEntries.find(v => v.key === key);
        if (!entry) {
          log('Vector entry not found in cache', 'error');
          return;
        }
        document.getElementById('vector-key').value = entry.key;
        document.getElementById('vector-data').value = entry.vectorRaw;
        switchTab('vector');
        log(`Viewing vector "${key}"`, 'info');
        return;
      }

      keyEl.value = key;
      try {
        const res = await fetch('/api/get?key=' + encodeURIComponent(key));
        const value = await res.text();
        valueEl.value = value;
        switchTab('operations');
        log(`Viewing key: ${key}`, 'info');
      } catch (err) {
        log('Error viewing entry: ' + err.message, 'error');
      }
    }

    async function deleteEntry(key, entryType = 'kv') {
      if (entryType === 'vector') {
        if (!confirm(`Remove cached vector "${key}" from Browse Data?`)) return;
        removeVectorEntry(key);
        log(`Removed cached vector "${key}"`, 'info');
        return;
      }

      if (!confirm(`Delete key "${key}"?`)) return;
      
      try {
        const res = await fetch('/api/delete', {
          method: 'POST',
          headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
          body: new URLSearchParams({ key })
        });
        
        if (res.ok) {
          log(`✓ Deleted "${key}"`, 'success');
          await refreshBrowse();
          await refreshStats();
        } else {
          log(`✗ Delete failed: ${await res.text()}`, 'error');
        }
      } catch (err) {
        log('Error: ' + err.message, 'error');
      }
    }

    function exportData() {
      const data = JSON.stringify(allEntries, null, 2);
      const blob = new Blob([data], { type: 'application/json' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = `vectis-export-${new Date().toISOString().split('T')[0]}.json`;
      a.click();
      log(`✓ Exported ${allEntries.length} entries`, 'success');
    }

    async function refreshFiles() {
      const treeEl = document.getElementById('file-tree');
      treeEl.innerHTML = '<div style="text-align: center; padding: 20px;">Loading files...</div>';
      
      try {
        const res = await fetch('/api/files');
        const data = await res.json();
        
        let html = '';
        for (const file of data.files) {
          html += `
            <div class="file-item">
              <span class="${file.is_dir ? 'folder' : 'file-name'}">${file.is_dir ? '📁' : '📄'} ${file.name}</span>
              <span class="file-size">${file.is_dir ? '' : formatBytes(file.size)}</span>
            </div>
          `;
        }
        
        treeEl.innerHTML = html || '<div class="empty-state">No files found</div>';
      } catch (err) {
        treeEl.innerHTML = '<div class="empty-state">Error loading files</div>';
        log('Failed to load files: ' + err.message, 'error');
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
        
        if (res.ok) {
          log(`✓ PUT "${key}"`, 'success');
          await refreshStats();
          await refreshBrowse();
        } else {
          log(`✗ PUT failed: ${await res.text()}`, 'error');
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
        
        if (res.ok) {
          const text = await res.text();
          log(`✓ GET "${key}" = "${text}"`, 'success');
          valueEl.value = text;
        } else if (res.status === 404) {
          log(`✗ Key "${key}" not found`, 'error');
        } else {
          log(`✗ GET failed: ${await res.text()}`, 'error');
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
        
        if (res.ok) {
          log(`✓ DELETE "${key}"`, 'success');
          await refreshStats();
        } else {
          log(`✗ DELETE failed: ${await res.text()}`, 'error');
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
        } catch (err) {}
      }
      
      log(`✓ Batch complete: ${success}/${lines.length}`, 'success');
      await refreshStats();
      await refreshBrowse();
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
        } catch (err) {}
      }
      
      const duration = ((Date.now() - startTime) / 1000).toFixed(2);
      log(`✓ Generated ${success}/${count} in ${duration}s`, 'success');
      await refreshStats();
      await refreshBrowse();
    }

    async function clearDatabase() {
      if (!confirm('Delete all entries? This cannot be undone!')) return;
      
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
        vectorEntries = [];
        await refreshBrowse();
      } catch (err) {
        log('Error clearing database: ' + err.message, 'error');
      }
    }

    function clearConsole() {
      consoleEl.innerHTML = 'Console cleared.\n';
    }
)HTML";

// Part 3: Vector operations JavaScript
static const char* kIndexHtml_Part3 = R"HTML(
    // ====== Vector Operations ======

    async function doPutVector() {
      const key = document.getElementById('vector-key').value.trim();
      const vectorData = document.getElementById('vector-data').value.trim();
      
      if (!key || !vectorData) {
        log('Key and vector data required', 'error');
        return;
      }

      try {
        const res = await fetch('/api/vector/put', {
          method: 'POST',
          headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
          body: new URLSearchParams({ key, vector: vectorData })
        });
        
        if (res.ok) {
          const dimension = vectorData.split(',').length;
          upsertVectorEntry(key, vectorData);
          log(`✓ Inserted vector "${key}" (${dimension}-dim)`, 'success');
          await refreshVectorStats();
          await refreshBrowse();
        } else {
          log(`✗ Vector PUT failed: ${await res.text()}`, 'error');
        }
      } catch (err) {
        log('Error: ' + err.message, 'error');
      }
    }

    async function doBulkVectorInsert() {
      if (!configuredVectorDimension || configuredVectorDimension <= 0) {
        log('Vector dimension not available yet. Try refreshing stats.', 'error');
        return;
      }

      const prefix = document.getElementById('bulk-vector-prefix').value.trim() || 'vector';
      const count = Math.max(1, parseInt(document.getElementById('bulk-vector-count').value) || 1);
      const range = Math.max(0.1, parseFloat(document.getElementById('bulk-vector-range').value) || 1);

      log(`Bulk inserting ${count} vectors with prefix "${prefix}"...`);
      let success = 0;
      const start = Date.now();

      for (let i = 0; i < count; i++) {
        const key = `${prefix}_${Date.now()}_${i}`;
        const values = buildRandomVector(configuredVectorDimension, range);
        const vectorPayload = values.join(',');

        try {
          const res = await fetch('/api/vector/put', {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body: new URLSearchParams({ key, vector: vectorPayload })
          });

          if (res.ok) {
            success++;
            upsertVectorEntry(key, vectorPayload);
          } else {
            log(`Vector insert failed for ${key}: ${await res.text()}`, 'error');
          }
        } catch (err) {
          log('Bulk vector insert error: ' + err.message, 'error');
        }

        if ((i + 1) % 25 === 0) {
          log(`  Progress ${i + 1}/${count}`, 'info');
        }
      }

      const duration = ((Date.now() - start) / 1000).toFixed(2);
      log(`✓ Bulk vector insert ${success}/${count} (range ±${range}) in ${duration}s`, 'success');
      await refreshVectorStats();
      await refreshBrowse();
    }

    async function doGetVector() {
      const key = document.getElementById('vector-key').value.trim();
      if (!key) {
        log('Key required', 'error');
        return;
      }

      try {
        const res = await fetch('/api/vector/get?key=' + encodeURIComponent(key));
        
        if (res.ok) {
          const vectorData = await res.text();
          document.getElementById('vector-data').value = vectorData;
          const dimension = vectorData.split(',').length;
          upsertVectorEntry(key, vectorData);
          log(`✓ Retrieved vector "${key}" (${dimension}-dim)`, 'success');
          await refreshBrowse();
        } else if (res.status === 404) {
          log(`✗ Vector "${key}" not found`, 'error');
        } else {
          log(`✗ Vector GET failed: ${await res.text()}`, 'error');
        }
      } catch (err) {
        log('Error: ' + err.message, 'error');
      }
    }

    async function doVectorSearch() {
      const queryVector = document.getElementById('query-vector').value.trim();
      const k = parseInt(document.getElementById('search-k').value);
      
      if (!queryVector) {
        log('Query vector required', 'error');
        return;
      }

      try {
        const res = await fetch('/api/vector/search?vector=' + encodeURIComponent(queryVector) + '&k=' + k);
        
        if (res.ok) {
          const data = await res.json();
          displaySearchResults(data.results);
          log(`✓ Found ${data.results.length} similar vectors`, 'success');
        } else {
          log(`✗ Vector search failed: ${await res.text()}`, 'error');
        }
      } catch (err) {
        log('Error: ' + err.message, 'error');
      }
    }

    function displaySearchResults(results) {
      const container = document.getElementById('search-results');
      
      if (results.length === 0) {
        container.innerHTML = '<div class="empty-state">No results found</div>';
        return;
      }

      let html = '<div style="background: #f8f9fa; border-radius: 6px; padding: 15px; margin-top: 10px;">';
      html += '<h4 style="margin: 0 0 10px 0; color: #667eea;">Search Results:</h4>';
      
      results.forEach((result, idx) => {
        const barWidth = Math.max(5, 100 - (result.distance * 10));
        html += `
          <div style="margin-bottom: 8px; padding: 10px; background: white; border-radius: 4px; border-left: 3px solid #667eea;">
            <div style="display: flex; justify-content: space-between; align-items: center;">
              <span style="font-weight: 600; font-family: monospace; color: #333;">${escapeHtml(result.key)}</span>
              <span style="font-size: 12px; color: #666;">distance: ${result.distance.toFixed(4)}</span>
            </div>
            <div style="margin-top: 5px; background: #e0e0e0; height: 4px; border-radius: 2px; overflow: hidden;">
              <div style="background: linear-gradient(90deg, #667eea, #764ba2); height: 100%; width: ${barWidth}%;"></div>
            </div>
          </div>
        `;
      });
      
      html += '</div>';
      container.innerHTML = html;
    }

    async function refreshVectorStats() {
      try {
        const res = await fetch('/api/vector/stats');
        const stats = await res.json();
        
        document.getElementById('vector-enabled').textContent = stats.index_enabled ? 'Yes' : 'No';
        document.getElementById('vector-count').textContent = stats.num_vectors;
        document.getElementById('vector-dimension').textContent = stats.dimension;
        document.getElementById('vector-metric').textContent = stats.metric || 'N/A';
        document.getElementById('vector-layers').textContent = stats.num_layers;

        const avgConnections = typeof stats.avg_connections === 'number' ? stats.avg_connections : 0;
        document.getElementById('vector-connections').textContent = avgConnections.toFixed(2);

        if (stats.index_enabled && stats.dimension > 0) {
          configuredVectorDimension = stats.dimension;
          document.getElementById('configured-dimension').textContent = stats.dimension;
          document.getElementById('random-dim-label').textContent = stats.dimension;
        }
        
        log('✓ Vector stats refreshed', 'info');
      } catch (err) {
        log('Failed to refresh vector stats: ' + err.message, 'error');
      }
    }

    function buildRandomVector(dimension, range = 1) {
      const values = [];
      for (let i = 0; i < dimension; i++) {
        const value = (Math.random() * 2 - 1) * range;
        values.push(value.toFixed(4));
      }
      return values;
    }

    function generateRandomVector(dimension = configuredVectorDimension) {
      if (!dimension || dimension <= 0) {
        log('Configured vector dimension is invalid', 'error');
        return;
      }
      const values = buildRandomVector(dimension);
      document.getElementById('vector-data').value = values.join(',');
      log(`Generated random ${dimension}-dimensional vector`, 'info');
    }

    function copyVectorToQuery() {
      const vectorData = document.getElementById('vector-data').value;
      document.getElementById('query-vector').value = vectorData;
      log('Copied vector to query field', 'info');
    }

    // Auto-refresh
    refreshStats();
    refreshVectorStats();
    refreshBrowse();
    refreshFiles();
    setInterval(refreshStats, 5000);
  </script>
</body>
</html>
)HTML";

// Combine the four parts
static const std::string kIndexHtml =
    std::string(kIndexHtml_Part1) + std::string(kIndexHtml_Part1b) +
    std::string(kIndexHtml_Part2a) + std::string(kIndexHtml_Part2b) + std::string(kIndexHtml_Part3);

int main(int argc, char** argv) {
  using core_engine::Engine;
  using core_engine::Log;
  using core_engine::LogLevel;

  const std::string db_dir = (argc >= 2) ? argv[1] : "./_web_demo";
  const int port = (argc >= 3) ? std::stoi(argv[2]) : 8080;

  std::size_t vector_dimension = 128;
  if (argc >= 4) {
    try {
      std::size_t parsed_dim = std::stoul(argv[3]);
      if (parsed_dim == 0) {
        Log(LogLevel::kWarn, "Vector dimension must be greater than zero; defaulting to 128");
      } else {
        vector_dimension = parsed_dim;
      }
    } catch (...) {
      Log(LogLevel::kWarn,
          "Invalid vector dimension '" + std::string(argv[3]) + "', defaulting to 128");
    }
  }

  Engine engine;

  auto config = core_engine::DatabaseConfig::Embedded(db_dir);
  config.enable_vector_index = true;
  config.vector_dimension = vector_dimension;
  Log(LogLevel::kInfo,
      "Vector index enabled (dimension=" + std::to_string(config.vector_dimension) + ")");

  auto status = engine.Open(config);

  if (!status.ok()) {
    Log(LogLevel::kError, status.ToString());
    return 1;
  }

  std::mutex engine_mutex;
  httplib::Server server;

  auto escape_json = [](const std::string& s) -> std::string {
    std::string result;
    result.reserve(s.size());
    for (char c : s) {
      switch (c) {
      case '"':
        result += "\\\"";
        break;
      case '\\':
        result += "\\\\";
        break;
      case '\n':
        result += "\\n";
        break;
      case '\r':
        result += "\\r";
        break;
      case '\t':
        result += "\\t";
        break;
      default:
        result += c;
      }
    }
    return result;
  };

  server.Get("/", [&](const httplib::Request&, httplib::Response& res) {
    res.set_content(kIndexHtml, "text/html; charset=utf-8");
  });

  server.Get("/dashboard", [&](const httplib::Request&, httplib::Response& res) {
    res.set_content(kIndexHtml, "text/html; charset=utf-8");
  });

  Log(LogLevel::kInfo, "Registering vector API endpoints...");

  // Vector PUT endpoint
  server.Post("/api/vector/put", [&](const httplib::Request& req, httplib::Response& res) {
    if (!req.has_param("key") || !req.has_param("vector")) {
      res.status = 400;
      res.set_content("Missing key or vector", "text/plain");
      return;
    }

    const auto key = req.get_param_value("key");
    const auto vector_str = req.get_param_value("vector");

    std::vector<float> values;
    std::istringstream iss(vector_str);
    std::string token;
    while (std::getline(iss, token, ',')) {
      try {
        values.push_back(std::stof(token));
      } catch (...) {
        res.status = 400;
        res.set_content("Invalid vector format", "text/plain");
        return;
      }
    }

    core_engine::vector::Vector vec(values);

    std::lock_guard<std::mutex> lock(engine_mutex);
    const auto status = engine.PutVector(key, vec);

    if (!status.ok()) {
      res.status = 500;
      res.set_content(status.ToString(), "text/plain");
      return;
    }

    res.set_content("OK", "text/plain");
  });

  // Vector GET endpoint
  server.Get("/api/vector/get", [&](const httplib::Request& req, httplib::Response& res) {
    if (!req.has_param("key")) {
      res.status = 400;
      res.set_content("Missing key", "text/plain");
      return;
    }

    const auto key = req.get_param_value("key");

    std::lock_guard<std::mutex> lock(engine_mutex);
    const auto vec_opt = engine.GetVector(key);

    if (!vec_opt.has_value()) {
      res.status = 404;
      res.set_content("NOT_FOUND", "text/plain");
      return;
    }

    std::ostringstream oss;
    const auto& vec = *vec_opt;
    for (std::size_t i = 0; i < vec.dimension(); ++i) {
      if (i > 0)
        oss << ",";
      oss << vec[i];
    }

    res.set_content(oss.str(), "text/plain");
  });

  // Vector SEARCH endpoint
  server.Get("/api/vector/search", [&](const httplib::Request& req, httplib::Response& res) {
    if (!req.has_param("vector")) {
      res.status = 400;
      res.set_content("Missing vector query", "text/plain");
      return;
    }

    const auto vector_str = req.get_param_value("vector");
    const auto k = req.has_param("k") ? std::stoi(req.get_param_value("k")) : 5;

    std::vector<float> values;
    std::istringstream iss(vector_str);
    std::string token;
    while (std::getline(iss, token, ',')) {
      try {
        values.push_back(std::stof(token));
      } catch (...) {
        res.status = 400;
        res.set_content("Invalid vector format", "text/plain");
        return;
      }
    }

    core_engine::vector::Vector query_vec(values);

    std::lock_guard<std::mutex> lock(engine_mutex);
    const auto results = engine.SearchSimilar(query_vec, k, false);

    std::ostringstream json;
    json << "{\"results\":[";

    bool first = true;
    for (const auto& result : results) {
      if (!first)
        json << ",";
      first = false;
      json << "{\"key\":\"" << result.key << "\",\"distance\":" << result.distance << "}";
    }

    json << "]}";
    res.set_content(json.str(), "application/json");
  });

  // Vector STATS endpoint
  server.Get("/api/vector/stats", [&](const httplib::Request&, httplib::Response& res) {
    std::lock_guard<std::mutex> lock(engine_mutex);
    const auto vstats = engine.GetVectorStats();

    std::ostringstream json;
    json << "{"
         << "\"index_enabled\":" << (vstats.index_enabled ? "true" : "false") << ","
         << "\"num_vectors\":" << vstats.num_vectors << ","
         << "\"dimension\":" << vstats.dimension << ","
         << "\"metric\":\"" << vstats.metric << "\","
         << "\"num_layers\":" << vstats.num_layers << ","
         << "\"avg_connections\":" << vstats.avg_connections_per_node << "}";

    res.set_content(json.str(), "application/json");
  });

  // Vector LIST endpoint (used by Browse tab)
  server.Get("/api/vector/list", [&](const httplib::Request&, httplib::Response& res) {
    std::lock_guard<std::mutex> lock(engine_mutex);
    const auto vectors = engine.GetAllVectors();

    auto vector_to_string = [](const core_engine::vector::Vector& vec) -> std::string {
      std::ostringstream oss;
      for (std::size_t i = 0; i < vec.dimension(); ++i) {
        if (i > 0) {
          oss << ",";
        }
        oss << vec[i];
      }
      return oss.str();
    };

    std::ostringstream json;
    json << "{\"vectors\":[";

    bool first = true;
    for (const auto& [key, vec] : vectors) {
      if (!first)
        json << ",";
      first = false;
      const auto serialized = vector_to_string(vec);
      json << "{\"key\":\"" << escape_json(key) << "\",";
      json << "\"dimension\":" << vec.dimension() << ",";
      json << "\"vector\":\"" << escape_json(serialized) << "\"}";
    }

    json << "]}";
    res.set_content(json.str(), "application/json");
  });

  Log(LogLevel::kInfo, "Vector API endpoints registered");

  server.Get("/api/stats", [&](const httplib::Request&, httplib::Response& res) {
    std::lock_guard<std::mutex> lock(engine_mutex);
    const auto stats = engine.GetStats();

    std::ostringstream json;
    json << "{"
         << "\"total_pages\":" << stats.total_pages << ","
         << "\"total_reads\":" << stats.total_reads << ","
         << "\"total_writes\":" << stats.total_writes << ","
         << "\"checksum_failures\":" << stats.checksum_failures << ","
         << "\"total_entries\":" << stats.total_entries << ","
         << "\"avg_get_time_us\":" << stats.avg_get_time_us << ","
         << "\"avg_put_time_us\":" << stats.avg_put_time_us << ","
         << "\"total_gets\":" << stats.total_gets << ","
         << "\"total_puts\":" << stats.total_puts << "}";

    res.set_content(json.str(), "application/json");
  });

  // Prometheus metrics endpoint
  server.Get("/metrics", [&](const httplib::Request&, httplib::Response& res) {
    std::lock_guard<std::mutex> lock(engine_mutex);

    // Update metrics from engine stats
    const auto stats = engine.GetStats();
    auto& metrics = core_engine::GetGlobalMetrics();

    // Set gauges for current values
    metrics.SetGauge("core_engine_total_pages", static_cast<double>(stats.total_pages));
    metrics.SetGauge("core_engine_total_reads", static_cast<double>(stats.total_reads));
    metrics.SetGauge("core_engine_total_writes", static_cast<double>(stats.total_writes));
    metrics.SetGauge("core_engine_checksum_failures", static_cast<double>(stats.checksum_failures));
    metrics.SetGauge("core_engine_avg_get_latency_microseconds",
                     static_cast<double>(stats.avg_get_time_us));
    metrics.SetGauge("core_engine_avg_put_latency_microseconds",
                     static_cast<double>(stats.avg_put_time_us));

    // Set counters for cumulative totals
    metrics.SetGauge("core_engine_total_gets", static_cast<double>(stats.total_gets));
    metrics.SetGauge("core_engine_total_puts", static_cast<double>(stats.total_puts));

    // Get Prometheus text format
    const std::string prometheus_text = metrics.GetPrometheusText();
    res.set_content(prometheus_text, "text/plain; version=0.0.4");
  });

  server.Get("/api/entries", [&](const httplib::Request&, httplib::Response& res) {
    std::lock_guard<std::mutex> lock(engine_mutex);
    const auto entries = engine.GetAllEntries();

    std::ostringstream json;
    json << "{\"entries\":[";

    bool first = true;
    for (const auto& [key, value] : entries) {
      if (!first)
        json << ",";
      first = false;
      json << "{\"key\":\"" << escape_json(key) << "\","
           << "\"value\":\"" << escape_json(value) << "\"}";
    }

    json << "]}";
    res.set_content(json.str(), "application/json");
  });

  server.Get("/api/files", [&](const httplib::Request&, httplib::Response& res) {
    std::ostringstream json;
    json << "{\"files\":[";

    bool first = true;
    try {
      if (fs::exists(db_dir)) {
        for (const auto& entry : fs::recursive_directory_iterator(db_dir)) {
          if (!first)
            json << ",";
          first = false;

          const auto path = entry.path();
          const auto relative = fs::relative(path, db_dir);
          const bool is_dir = fs::is_directory(entry);
          const auto size = is_dir ? 0 : fs::file_size(entry);

          json << "{\"name\":\"" << relative.string() << "\","
               << "\"is_dir\":" << (is_dir ? "true" : "false") << ","
               << "\"size\":" << size << "}";
        }
      }
    } catch (...) {
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

  // DEBUG endpoint to check internal state
  server.Get("/api/debug/keys", [&](const httplib::Request&, httplib::Response& res) {
    std::lock_guard<std::mutex> lock(engine_mutex);
    const auto entries = engine.GetAllEntries();
    std::ostringstream json;
    json << "{\"count\":" << entries.size() << "}";
    res.set_content(json.str(), "application/json");
  });

  // TEST endpoint
  server.Get("/api/test", [&](const httplib::Request&, httplib::Response& res) {
    res.set_content("TEST_OK", "text/plain");
  });

  Log(LogLevel::kInfo, "Enhanced web interface running");
  Log(LogLevel::kInfo, "Open http://localhost:" + std::to_string(port) + "/");
  Log(LogLevel::kInfo, "Access from network: http://<your-ip>:" + std::to_string(port) + "/");
  Log(LogLevel::kInfo, "Database: " + db_dir);

  server.listen("0.0.0.0", port);
  return 0;
}
