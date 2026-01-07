"""
Vectis Database Python Client SDK

A high-performance page-oriented vector database for AI/ML applications.

Example usage:
    >>> from vectis import VectisClient
    >>> 
    >>> # Connect to database
    >>> client = VectisClient("http://localhost:8080")
    >>> 
    >>> # Store key-value pairs
    >>> client.put("user:123", "John Doe")
    >>> 
    >>> # Retrieve data
    >>> value = client.get("user:123")
    >>> print(value)  # "John Doe"
    >>> 
    >>> # Store vector embeddings
    >>> embedding = [0.1, 0.5, 0.3, 0.8, 0.2]  # 5-dimensional vector
    >>> client.put_vector("doc:article1", embedding)
    >>> 
    >>> # Search for similar vectors
    >>> query = [0.2, 0.4, 0.3, 0.7, 0.1]
    >>> results = client.search_similar(query, k=5)
    >>> for key, distance in results:
    ...     print(f"{key}: {distance}")
"""

__version__ = "1.5.0"
__author__ = "Vectis Team"

from .client import VectisClient, VectisError, VectisConnectionError, VectisNotFoundError
from .vector import Vector

__all__ = [
    "VectisClient",
    "VectisError",
    "VectisConnectionError",
    "VectisNotFoundError",
    "Vector",
]
