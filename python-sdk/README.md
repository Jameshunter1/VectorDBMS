# Vectis Python SDK

Official Python client for Vectis - A high-performance page-oriented vector database.

An SDK is a set of tools, libraries, documentation, and code samples that help developers create applications for a specific platform or service. The Vectis Python SDK provides a convenient interface to interact with the Vectis database from Python applications.

## Installation

```bash
pip install vectis
```

Or install from source:

```bash
git clone https://github.com/yourusername/VectorDBMS
cd VectorDBMS/python-sdk
pip install -e .
```

## Quick Start

### Basic Key-Value Operations

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
```

### Batch Operations

```python
# Batch insert
client.batch_put({
    "product:1": "Laptop",
    "product:2": "Mouse",
    "product:3": "Keyboard"
})

# Batch retrieve
results = client.batch_get(["product:1", "product:2"])
for key, value in results.items():
    print(f"{key}: {value}")
```

### Range Scans

```python
# Scan a key range
results = client.scan("user:000", "user:999", limit=10)
for key, value in results:
    print(f"{key}: {value}")

# Reverse scan
results = client.scan("user:000", "user:999", reverse=True, limit=5)
```

### Vector Operations

```python
# Store vector embeddings
embedding1 = [0.1, 0.5, 0.3, 0.8, 0.2]  # 5D vector
client.put_vector("doc:article1", embedding1)

embedding2 = [0.2, 0.4, 0.3, 0.7, 0.1]
client.put_vector("doc:article2", embedding2)

# Search for similar vectors
query = [0.15, 0.45, 0.3, 0.75, 0.15]
results = client.search_similar(query, k=5)

for key, distance in results:
    print(f"{key}: distance={distance:.4f}")
```

### Working with Vectors

```python
from vectis import Vector
import numpy as np

# Create vectors
v1 = Vector([1.0, 2.0, 3.0])
v2 = Vector(np.array([4.0, 5.0, 6.0]))

# Normalize vectors
v1_normalized = v1.normalize()

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
    client.put("key", "value")
    value = client.get("key")
```

### Monitoring & Health

```python
# Get database statistics
stats = client.get_stats()
print(f"Total pages: {stats['total_pages']}")
print(f"Total puts: {stats['total_puts']}")

# Health check
health = client.health_check()
print(f"Status: {health['status']}")
```

## Complete Example: Semantic Search

```python
from vectis import VectisClient
from sentence_transformers import SentenceTransformer

# Initialize
client = VectisClient("http://localhost:8080")
model = SentenceTransformer('all-MiniLM-L6-v2')

# Index documents
documents = {
    "doc:1": "The quick brown fox jumps over the lazy dog",
    "doc:2": "Machine learning is transforming technology",
    "doc:3": "Databases store and retrieve data efficiently",
    "doc:4": "Vector embeddings represent text as numbers"
}

for doc_id, text in documents.items():
    # Generate embedding
    embedding = model.encode(text).tolist()
    
    # Store both text and embedding
    client.put(doc_id, text)
    client.put_vector(doc_id, embedding)

# Search
query = "How do vector databases work?"
query_embedding = model.encode(query).tolist()

results = client.search_similar(query_embedding, k=3)

print(f"Query: {query}\n")
for doc_id, distance in results:
    text = client.get(doc_id)
    print(f"{doc_id} (distance={distance:.4f}):")
    print(f"  {text}\n")
```

## API Reference

### VectisClient

#### Constructor
- `VectisClient(base_url: str, timeout: int = 30)`

#### Key-Value Methods
- `put(key: str, value: str) -> None`
- `get(key: str) -> Optional[str]`
- `delete(key: str) -> None`
- `batch_put(items: Dict[str, str]) -> None`
- `batch_get(keys: List[str]) -> Dict[str, Optional[str]]`
- `scan(start: str, end: str, limit: int = 0, reverse: bool = False) -> List[Tuple[str, str]]`

#### Vector Methods
- `put_vector(key: str, vector: List[float]) -> None`
- `get_vector(key: str) -> Optional[Vector]`
- `search_similar(query: List[float], k: int = 10) -> List[Tuple[str, float]]`

#### Monitoring
- `get_stats() -> Dict[str, Any]`
- `health_check() -> Dict[str, Any]`

### Vector Class

#### Methods
- `normalize() -> Vector`
- `cosine_similarity(other: Vector) -> float`
- `euclidean_distance(other: Vector) -> float`
- `dot_product(other: Vector) -> float`
- `to_list() -> List[float]`
- `to_numpy() -> np.ndarray`

## Error Handling

```python
from vectis import VectisClient, VectisError, VectisConnectionError, VectisNotFoundError

try:
    client = VectisClient("http://localhost:8080")
    client.put("key", "value")
except VectisConnectionError:
    print("Failed to connect to database")
except VectisError as e:
    print(f"Database error: {e}")
```

## License

MIT License - see LICENSE file for details.
