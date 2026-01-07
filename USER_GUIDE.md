# Vectis Database - Complete User Guide

## Welcome to Vectis! 🚀

Vectis is a high-performance, page-oriented vector database designed for production AI/ML applications. This guide will walk you through everything from installation to building a complete semantic search application.

---

## Table of Contents

1. [Installation](#installation)
2. [Quick Start (5 Minutes)](#quick-start-5-minutes)
3. [Interactive CLI Tutorial](#interactive-cli-tutorial)
4. [Python SDK Tutorial](#python-sdk-tutorial)
5. [Building a Semantic Search App](#building-a-semantic-search-app)
6. [Web API Reference](#web-api-reference)
7. [Production Deployment](#production-deployment)
8. [Performance Tuning](#performance-tuning)
9. [Troubleshooting](#troubleshooting)

---

## Installation

### Option 1: Docker (Recommended for Quick Start)

```bash
# Pull and run Vectis in one command
docker run -d \
  --name vectis \
  -p 8080:8080 \
  -v vectis-data:/vectis/data \
  vectis/database:latest

# Verify it's running
curl http://localhost:8080/api/health
```

### Option 2: Docker Compose (Full Stack)

```bash
git clone https://github.com/yourusername/VectorDBMS
cd VectorDBMS

# Start Vectis + Prometheus + Grafana
docker-compose up -d

# Access:
# - Vectis API: http://localhost:8080
# - Grafana Dashboard: http://localhost:3000 (admin/admin)
# - Prometheus: http://localhost:9090
```

### Option 3: Build from Source (Windows)

```powershell
# Clone repository
git clone https://github.com/yourusername/VectorDBMS
cd VectorDBMS

# Configure and build
cmake --preset windows-vs2022-x64-release -S src
cmake --build build/windows-vs2022-x64-release --config Release -j 8

# Run tests
ctest --test-dir build/windows-vs2022-x64-release -C Release --output-on-failure

# Executables will be in:
# - build/windows-vs2022-x64-release/Release/dbcli.exe
# - build/windows-vs2022-x64-release/Release/dbweb.exe
```

### Option 4: Build from Source (Linux)

```bash
# Install dependencies
sudo apt-get update
sudo apt-get install -y build-essential cmake git ninja-build

# Clone and build
git clone https://github.com/yourusername/VectorDBMS
cd VectorDBMS
cmake -B build -S src -DCMAKE_BUILD_TYPE=Release -GNinja
cmake --build build -j$(nproc)

# Run tests
ctest --test-dir build --output-on-failure

# Executables in: build/dbcli, build/dbweb
```

---

## Quick Start (5 Minutes)

### Step 1: Start the Server

```bash
# Using Docker
docker run -d -p 8080:8080 vectis/database:latest

# OR using built executable
./build/dbweb ./my_database 8080
```

### Step 2: Store Some Data (HTTP API)

```bash
# PUT a key-value pair
curl -X POST http://localhost:8080/api/put \
  -d "key=user:123" \
  -d "value=John Doe"

# GET the value
curl http://localhost:8080/api/get?key=user:123
# Output: John Doe

# DELETE a key
curl -X POST http://localhost:8080/api/delete \
  -d "key=user:123"
```

### Step 3: Try Vector Search

```bash
# Store a vector (5-dimensional embedding)
curl -X POST http://localhost:8080/api/vector/put \
  -H "Content-Type: application/json" \
  -d '{
    "key": "doc:1",
    "vector": [0.1, 0.5, 0.3, 0.8, 0.2]
  }'

# Search for similar vectors
curl -X POST http://localhost:8080/api/vector/search \
  -H "Content-Type: application/json" \
  -d '{
    "query": [0.2, 0.4, 0.3, 0.7, 0.1],
    "k": 5
  }'
```

---

## Interactive CLI Tutorial

The Vectis CLI provides an interactive REPL (Read-Eval-Print Loop) for exploring your database.

### Starting the CLI

```bash
# Windows
.\build\Release\dbcli.exe .\my_database

# Linux
./build/dbcli ./my_database
```

You'll see:

```
╔══════════════════════════════════════════════════════════════╗
║                                                              ║
║              VECTIS DATABASE - Interactive CLI               ║
║                    Production Version 1.5                    ║
║                                                              ║
║  High-Performance Page-Oriented Vector Database Engine      ║
║                                                              ║
╚══════════════════════════════════════════════════════════════╝

Database: ./my_database
Status:   Connected ✓

Type 'help' for command list or 'quit' to exit.

vectis>
```

### Basic Commands

```bash
# Get help
vectis> help

# Store data
vectis> put user:alice "Alice Johnson"
✓ Stored: user:alice

vectis> put user:bob "Bob Smith"
✓ Stored: user:bob

# Retrieve data
vectis> get user:alice
✓ user:alice = Alice Johnson

# Delete data
vectis> delete user:bob
✓ Deleted: user:bob

# Scan a range
vectis> scan user:000 user:999 10
✓ Found 1 entries:
  user:alice = Alice Johnson
```

### Batch Operations

```bash
# Batch insert (key:value pairs)
vectis> bput name:Alice age:30 city:NYC role:Engineer
✓ Batch inserted 4 entries

# Batch get
vectis> bget name:Alice age:30 city:NYC
  name:Alice = Alice
  age:30 = 30
  city:NYC = NYC
```

### Statistics and Monitoring

```bash
vectis> stats

╔══════════════════════════════════════════════════════════════╗
║                     DATABASE STATISTICS                      ║
╚══════════════════════════════════════════════════════════════╝

Storage:
  Total Pages:                   42
  Database Size:                168 KB

Operations:
  Total Puts:                   150
  Total Gets:                    75
  Total Deletes:                  5

Performance:
  Avg Get Latency:             23.5 μs
  Avg Put Latency:             45.2 μs

Memory:
  MemTable Size:               128 KB
  WAL Size:                     64 KB

vectis> info

Database Information:
  Path:              ./my_database
  Engine Version:    1.5.0
  Page Size:         4096 bytes
  Vector Support:    Enabled (HNSW)
  WAL Enabled:       Yes
  Compaction:        Automatic
```

---

## Python SDK Tutorial

### Installation

```bash
pip install vectis
```

### Basic Usage

```python
from vectis import VectisClient

# Connect to database
client = VectisClient("http://localhost:8080")

# Store data
client.put("user:123", "John Doe")
client.put("user:456", "Jane Smith")

# Retrieve data
value = client.get("user:123")
print(value)  # "John Doe"

# Delete data
client.delete("user:456")

# Check if key exists
value = client.get("user:456")
print(value)  # None
```

### Batch Operations

```python
# Batch insert
client.batch_put({
    "product:1": "Laptop",
    "product:2": "Mouse",
    "product:3": "Keyboard",
    "product:4": "Monitor"
})

# Batch retrieve
keys = ["product:1", "product:2", "product:3"]
results = client.batch_get(keys)

for key, value in results.items():
    if value:
        print(f"{key}: {value}")
```

### Range Scans

```python
# Scan all products
results = client.scan("product:0", "product:9", limit=10)

for key, value in results:
    print(f"{key}: {value}")

# Reverse scan
results = client.scan("product:0", "product:9", reverse=True, limit=5)
```

### Vector Operations

```python
# Store vector embeddings
embedding1 = [0.1, 0.5, 0.3, 0.8, 0.2, 0.6, 0.4, 0.7]
client.put_vector("doc:1", embedding1)

embedding2 = [0.2, 0.4, 0.3, 0.7, 0.1, 0.5, 0.3, 0.6]
client.put_vector("doc:2", embedding2)

# Search for similar vectors
query = [0.15, 0.45, 0.3, 0.75, 0.15, 0.55, 0.35, 0.65]
results = client.search_similar(query, k=5)

for key, distance in results:
    print(f"{key}: distance={distance:.4f}")
```

### Working with Vector Class

```python
from vectis import Vector
import numpy as np

# Create vectors
v1 = Vector([1.0, 2.0, 3.0, 4.0])
v2 = Vector([2.0, 3.0, 4.0, 5.0])

# Normalize
v1_norm = v1.normalize()
print(f"Original: {v1.to_list()}")
print(f"Normalized: {v1_norm.to_list()}")

# Compute similarity
similarity = v1.cosine_similarity(v2)
print(f"Cosine similarity: {similarity:.4f}")

# Compute distance
distance = v1.euclidean_distance(v2)
print(f"Euclidean distance: {distance:.4f}")
```

### Context Manager

```python
# Automatically close connection
with VectisClient("http://localhost:8080") as client:
    client.put("temp:data", "temporary value")
    value = client.get("temp:data")
    print(value)
# Connection automatically closed here
```

---

## Building a Semantic Search App

Let's build a complete semantic search application using Vectis and the `sentence-transformers` library.

### Step 1: Install Dependencies

```bash
pip install vectis sentence-transformers
```

### Step 2: Create the Application

```python
# semantic_search.py
from vectis import VectisClient
from sentence_transformers import SentenceTransformer
import time

class SemanticSearchEngine:
    def __init__(self, db_url="http://localhost:8080", model_name='all-MiniLM-L6-v2'):
        """Initialize the search engine."""
        self.client = VectisClient(db_url)
        self.model = SentenceTransformer(model_name)
        print(f"✓ Connected to Vectis at {db_url}")
        print(f"✓ Loaded model: {model_name}")
    
    def index_document(self, doc_id: str, text: str):
        """Index a single document."""
        # Generate embedding
        embedding = self.model.encode(text).tolist()
        
        # Store both text and embedding
        self.client.put(doc_id, text)
        self.client.put_vector(doc_id, embedding)
        
        print(f"✓ Indexed: {doc_id}")
    
    def index_documents(self, documents: dict):
        """Index multiple documents."""
        print(f"\nIndexing {len(documents)} documents...")
        start = time.time()
        
        for doc_id, text in documents.items():
            self.index_document(doc_id, text)
        
        elapsed = time.time() - start
        print(f"✓ Indexed {len(documents)} documents in {elapsed:.2f}s")
    
    def search(self, query: str, k: int = 5):
        """Search for documents similar to query."""
        # Generate query embedding
        query_embedding = self.model.encode(query).tolist()
        
        # Search for similar vectors
        results = self.client.search_similar(query_embedding, k=k)
        
        # Retrieve full text for each result
        search_results = []
        for doc_id, distance in results:
            text = self.client.get(doc_id)
            search_results.append({
                'doc_id': doc_id,
                'text': text,
                'distance': distance,
                'similarity': 1 - distance  # Convert distance to similarity score
            })
        
        return search_results
    
    def print_results(self, query: str, results: list):
        """Pretty print search results."""
        print(f"\n{'='*70}")
        print(f"Query: {query}")
        print(f"{'='*70}\n")
        
        for i, result in enumerate(results, 1):
            print(f"{i}. {result['doc_id']} (similarity: {result['similarity']:.4f})")
            print(f"   {result['text']}")
            print()

# Example usage
if __name__ == "__main__":
    # Initialize
    engine = SemanticSearchEngine()
    
    # Index sample documents
    documents = {
        "doc:1": "The quick brown fox jumps over the lazy dog",
        "doc:2": "Machine learning is transforming modern technology",
        "doc:3": "Databases store and retrieve data efficiently",
        "doc:4": "Vector embeddings represent text as numerical arrays",
        "doc:5": "Python is a popular programming language for data science",
        "doc:6": "Artificial intelligence systems learn from experience",
        "doc:7": "Cloud computing provides scalable infrastructure",
        "doc:8": "Neural networks are inspired by biological brains",
        "doc:9": "Natural language processing enables computers to understand text",
        "doc:10": "Search engines index web pages for quick retrieval"
    }
    
    engine.index_documents(documents)
    
    # Search examples
    queries = [
        "How do vector databases work?",
        "What is machine learning?",
        "Tell me about programming languages",
        "How do search engines work?"
    ]
    
    for query in queries:
        results = engine.search(query, k=3)
        engine.print_results(query, results)
```

### Step 3: Run the Application

```bash
# Start Vectis server (if not running)
docker run -d -p 8080:8080 vectis/database:latest

# Run the semantic search app
python semantic_search.py
```

Output:

```
✓ Connected to Vectis at http://localhost:8080
✓ Loaded model: all-MiniLM-L6-v2

Indexing 10 documents...
✓ Indexed: doc:1
✓ Indexed: doc:2
...
✓ Indexed 10 documents in 2.34s

======================================================================
Query: How do vector databases work?
======================================================================

1. doc:4 (similarity: 0.8524)
   Vector embeddings represent text as numerical arrays

2. doc:3 (similarity: 0.7892)
   Databases store and retrieve data efficiently

3. doc:9 (similarity: 0.7543)
   Natural language processing enables computers to understand text

...
```

### Step 4: Advanced Features

```python
# Add filtering by metadata
def index_with_metadata(self, doc_id: str, text: str, metadata: dict):
    """Index document with metadata."""
    embedding = self.model.encode(text).tolist()
    
    # Store text
    self.client.put(doc_id, text)
    
    # Store metadata
    for key, value in metadata.items():
        self.client.put(f"{doc_id}:meta:{key}", str(value))
    
    # Store embedding
    self.client.put_vector(doc_id, embedding)

# Search with metadata filtering
def search_with_filter(self, query: str, category: str, k: int = 5):
    """Search within a specific category."""
    results = self.search(query, k=k*2)  # Get more results
    
    # Filter by category
    filtered = []
    for result in results:
        doc_category = self.client.get(f"{result['doc_id']}:meta:category")
        if doc_category == category:
            filtered.append(result)
            if len(filtered) >= k:
                break
    
    return filtered
```

---

## Web API Reference

### Base URL
```
http://localhost:8080/api
```

### Authentication (if enabled)
```bash
# Login to get session token
curl -X POST http://localhost:8080/api/auth/login \
  -d "username=admin" \
  -d "password=password"

# Use token in subsequent requests
curl -H "Authorization: Bearer <token>" \
  http://localhost:8080/api/stats
```

### Endpoints

#### Health Check
```bash
GET /api/health
```

Response:
```json
{
  "status": "healthy",
  "database_open": true,
  "version": "1.5.0"
}
```

#### PUT - Store Key-Value
```bash
POST /api/put
Content-Type: application/x-www-form-urlencoded

key=mykey&value=myvalue
```

#### GET - Retrieve Value
```bash
GET /api/get?key=mykey
```

#### DELETE - Remove Key
```bash
POST /api/delete
Content-Type: application/x-www-form-urlencoded

key=mykey
```

#### SCAN - Range Query
```bash
GET /api/scan?start=key:000&end=key:999&limit=10&reverse=false
```

#### Vector PUT
```bash
POST /api/vector/put
Content-Type: application/json

{
  "key": "doc:1",
  "vector": [0.1, 0.5, 0.3, 0.8, 0.2]
}
```

#### Vector Search
```bash
POST /api/vector/search
Content-Type: application/json

{
  "query": [0.2, 0.4, 0.3, 0.7, 0.1],
  "k": 5
}
```

Response:
```json
{
  "results": [
    {"key": "doc:1", "distance": 0.156},
    {"key": "doc:3", "distance": 0.234}
  ]
}
```

#### Statistics
```bash
GET /api/stats
```

Response:
```json
{
  "total_pages": 1042,
  "total_puts": 15000,
  "total_gets": 8000,
  "memtable_size_bytes": 131072,
  "avg_get_latency_us": 23.5
}
```

#### Metrics (Prometheus format)
```bash
GET /metrics
```

---

## Production Deployment

### Docker Deployment

#### Single Instance
```bash
docker run -d \
  --name vectis \
  --restart unless-stopped \
  -p 8080:8080 \
  -v /data/vectis:/vectis/data \
  -e VECTIS_BUFFER_POOL_SIZE_MB=1024 \
  -e VECTIS_ENABLE_VECTOR_INDEX=true \
  -e VECTIS_VECTOR_DIMENSION=384 \
  vectis/database:latest
```

#### Docker Compose with Monitoring
```bash
# Start full stack
docker-compose up -d

# View logs
docker-compose logs -f vectis

# Scale (future support)
docker-compose up -d --scale vectis=3
```

### Configuration

Create `vectis.conf`:

```ini
[server]
host = 0.0.0.0
port = 8080
max_connections = 100

[database]
data_dir = /var/lib/vectis/data
buffer_pool_size_mb = 1024
wal_buffer_size_kb = 256

[vector]
enable_vector_index = true
vector_dimension = 384
hnsw_m = 16
hnsw_ef_construction = 200
hnsw_ef_search = 50

[security]
require_authentication = true
session_timeout_minutes = 30
audit_log_path = /var/log/vectis/audit.log
```

### Systemd Service (Linux)

Create `/etc/systemd/system/vectis.service`:

```ini
[Unit]
Description=Vectis Database Server
After=network.target

[Service]
Type=simple
User=vectis
Group=vectis
WorkingDirectory=/opt/vectis
ExecStart=/opt/vectis/dbweb /var/lib/vectis/data 8080
Restart=on-failure
RestartSec=5s

[Install]
WantedBy=multi-user.target
```

Enable and start:

```bash
sudo systemctl enable vectis
sudo systemctl start vectis
sudo systemctl status vectis
```

---

## Performance Tuning

### Buffer Pool Size

```bash
# Small dataset (< 1GB)
VECTIS_BUFFER_POOL_SIZE_MB=256

# Medium dataset (1-10GB)
VECTIS_BUFFER_POOL_SIZE_MB=1024

# Large dataset (> 10GB)
VECTIS_BUFFER_POOL_SIZE_MB=4096
```

### Vector Index Parameters

```bash
# High recall (slower indexing, better search quality)
VECTIS_HNSW_M=32
VECTIS_HNSW_EF_CONSTRUCTION=400
VECTIS_HNSW_EF_SEARCH=100

# Fast indexing (faster writes, lower recall)
VECTIS_HNSW_M=8
VECTIS_HNSW_EF_CONSTRUCTION=100
VECTIS_HNSW_EF_SEARCH=25

# Balanced (recommended)
VECTIS_HNSW_M=16
VECTIS_HNSW_EF_CONSTRUCTION=200
VECTIS_HNSW_EF_SEARCH=50
```

### Benchmarking

```bash
# Run built-in benchmarks
./build/benchmarks/core_engine_benchmarks

# Benchmark specific operations
./build/benchmarks/core_engine_benchmarks --benchmark_filter=Page
./build/benchmarks/core_engine_benchmarks --benchmark_filter=Vector
```

---

## Troubleshooting

### Database won't start

```bash
# Check logs
docker logs vectis

# Check data directory permissions
ls -la /var/lib/vectis/data

# Verify port is not in use
netstat -an | grep 8080
```

### Slow queries

```bash
# Check statistics
curl http://localhost:8080/api/stats

# Monitor Prometheus metrics
open http://localhost:9090

# View Grafana dashboard
open http://localhost:3000
```

### Out of memory

```bash
# Increase buffer pool
docker run -e VECTIS_BUFFER_POOL_SIZE_MB=2048 ...

# Check memory usage
docker stats vectis
```

### Connection refused

```bash
# Check if server is running
curl http://localhost:8080/api/health

# Check firewall
sudo ufw status
sudo firewall-cmd --list-all
```

---

## Next Steps

- 📚 Read the [API Reference](API_REFERENCE.md)
- 🏗️ Check out [Architecture Guide](ARCHITECTURE.md)
- 🔐 Review [Security Best Practices](SECURITY.md)
- 📊 Explore [Performance Benchmarks](PERFORMANCE.md)
- 🤝 Join our [Community](CONTRIBUTING.md)

---

## Support

- 📧 Email: support@vectis.dev
- 💬 Discord: [discord.gg/vectis](https://discord.gg/vectis)
- 🐛 Issues: [GitHub Issues](https://github.com/yourusername/VectorDBMS/issues)
- 📖 Docs: [vectis.readthedocs.io](https://vectis.readthedocs.io)

---

**Happy Building!** 🎉
