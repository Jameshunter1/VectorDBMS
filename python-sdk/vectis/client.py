"""
Vectis Database Python Client

Provides a high-level interface for interacting with Vectis database over HTTP.
"""

import json
import requests
from typing import Optional, List, Tuple, Dict, Any
from urllib.parse import urljoin, urlencode

from .vector import Vector


class VectisError(Exception):
    """Base exception for Vectis client errors."""
    pass


class VectisConnectionError(VectisError):
    """Raised when connection to database fails."""
    pass


class VectisNotFoundError(VectisError):
    """Raised when a key is not found in the database."""
    pass


class VectisClient:
    """
    Vectis Database Client
    
    High-level Python interface for Vectis vector database.
    
    Args:
        base_url: Base URL of the Vectis server (e.g., "http://localhost:8080")
        timeout: Request timeout in seconds (default: 30)
        
    Example:
        >>> client = VectisClient("http://localhost:8080")
        >>> client.put("key1", "value1")
        >>> value = client.get("key1")
        >>> print(value)
        value1
    """
    
    def __init__(self, base_url: str = "http://localhost:8080", timeout: int = 30):
        self.base_url = base_url.rstrip('/')
        self.timeout = timeout
        self.session = requests.Session()
        
        # Test connection
        try:
            self.get_stats()
        except Exception as e:
            raise VectisConnectionError(f"Failed to connect to {base_url}: {e}")
    
    def _request(self, method: str, endpoint: str, **kwargs) -> requests.Response:
        """Make HTTP request to the database."""
        url = urljoin(self.base_url, endpoint)
        
        try:
            response = self.session.request(
                method,
                url,
                timeout=self.timeout,
                **kwargs
            )
            return response
        except requests.exceptions.RequestException as e:
            raise VectisConnectionError(f"Request failed: {e}")
    
    def put(self, key: str, value: str) -> None:
        """
        Store a key-value pair.
        
        Args:
            key: The key to store
            value: The value to store
            
        Raises:
            VectisError: If the operation fails
        """
        response = self._request(
            'POST',
            '/api/put',
            data={'key': key, 'value': value}
        )
        
        if response.status_code != 200:
            raise VectisError(f"PUT failed: {response.text}")
    
    def get(self, key: str) -> Optional[str]:
        """
        Retrieve a value by key.
        
        Args:
            key: The key to retrieve
            
        Returns:
            The value associated with the key, or None if not found
        """
        response = self._request(
            'GET',
            '/api/get',
            params={'key': key}
        )
        
        if response.status_code == 404:
            return None
        elif response.status_code != 200:
            raise VectisError(f"GET failed: {response.text}")
        
        return response.text
    
    def delete(self, key: str) -> None:
        """
        Delete a key.
        
        Args:
            key: The key to delete
            
        Raises:
            VectisError: If the operation fails
        """
        response = self._request(
            'POST',
            '/api/delete',
            data={'key': key}
        )
        
        if response.status_code != 200:
            raise VectisError(f"DELETE failed: {response.text}")
    
    def batch_put(self, items: Dict[str, str]) -> None:
        """
        Store multiple key-value pairs in a single batch operation.
        
        Args:
            items: Dictionary of key-value pairs to store
            
        Example:
            >>> client.batch_put({
            ...     "user:1": "Alice",
            ...     "user:2": "Bob",
            ...     "user:3": "Charlie"
            ... })
        """
        operations = [
            {"type": "PUT", "key": k, "value": v}
            for k, v in items.items()
        ]
        
        response = self._request(
            'POST',
            '/api/batch',
            json={'operations': operations}
        )
        
        if response.status_code != 200:
            raise VectisError(f"Batch PUT failed: {response.text}")
    
    def batch_get(self, keys: List[str]) -> Dict[str, Optional[str]]:
        """
        Retrieve multiple values in a single batch operation.
        
        Args:
            keys: List of keys to retrieve
            
        Returns:
            Dictionary mapping keys to values (None for missing keys)
        """
        response = self._request(
            'POST',
            '/api/batch_get',
            json={'keys': keys}
        )
        
        if response.status_code != 200:
            raise VectisError(f"Batch GET failed: {response.text}")
        
        results = response.json()
        return {k: v if v else None for k, v in zip(keys, results.get('values', []))}
    
    def scan(self, start: str, end: str, limit: int = 0, reverse: bool = False) -> List[Tuple[str, str]]:
        """
        Scan a range of keys.
        
        Args:
            start: Start key (inclusive)
            end: End key (exclusive)
            limit: Maximum number of results (0 = no limit)
            reverse: Scan in reverse order
            
        Returns:
            List of (key, value) tuples
        """
        params = {
            'start': start,
            'end': end,
            'limit': limit,
            'reverse': str(reverse).lower()
        }
        
        response = self._request('GET', '/api/scan', params=params)
        
        if response.status_code != 200:
            raise VectisError(f"SCAN failed: {response.text}")
        
        results = response.json()
        return [(item['key'], item['value']) for item in results.get('entries', [])]
    
    def put_vector(self, key: str, vector: List[float]) -> None:
        """
        Store a vector embedding.
        
        Args:
            key: The key to store the vector under
            vector: List of floats representing the vector
            
        Example:
            >>> embedding = [0.1, 0.5, 0.3, 0.8, 0.2]
            >>> client.put_vector("doc:article1", embedding)
        """
        vec = Vector(vector)
        
        response = self._request(
            'POST',
            '/api/vector/put',
            json={'key': key, 'vector': vec.to_list()}
        )
        
        if response.status_code != 200:
            raise VectisError(f"Vector PUT failed: {response.text}")
    
    def get_vector(self, key: str) -> Optional[Vector]:
        """
        Retrieve a vector by key.
        
        Args:
            key: The key of the vector to retrieve
            
        Returns:
            Vector object, or None if not found
        """
        response = self._request(
            'GET',
            '/api/vector/get',
            params={'key': key}
        )
        
        if response.status_code == 404:
            return None
        elif response.status_code != 200:
            raise VectisError(f"Vector GET failed: {response.text}")
        
        data = response.json()
        return Vector(data['vector'])
    
    def search_similar(self, query: List[float], k: int = 10) -> List[Tuple[str, float]]:
        """
        Search for k nearest neighbor vectors.
        
        Args:
            query: Query vector as list of floats
            k: Number of results to return
            
        Returns:
            List of (key, distance) tuples, sorted by distance (lower = more similar)
            
        Example:
            >>> query = [0.2, 0.4, 0.3, 0.7, 0.1]
            >>> results = client.search_similar(query, k=5)
            >>> for key, distance in results:
            ...     print(f"{key}: distance={distance:.4f}")
        """
        vec = Vector(query)
        
        response = self._request(
            'POST',
            '/api/vector/search',
            json={'query': vec.to_list(), 'k': k}
        )
        
        if response.status_code != 200:
            raise VectisError(f"Vector search failed: {response.text}")
        
        data = response.json()
        return [(item['key'], item['distance']) for item in data.get('results', [])]
    
    def get_stats(self) -> Dict[str, Any]:
        """
        Get database statistics.
        
        Returns:
            Dictionary containing database statistics
        """
        response = self._request('GET', '/api/stats')
        
        if response.status_code != 200:
            raise VectisError(f"Failed to get stats: {response.text}")
        
        return response.json()
    
    def health_check(self) -> Dict[str, Any]:
        """
        Check database health status.
        
        Returns:
            Dictionary containing health status information
        """
        response = self._request('GET', '/api/health')
        
        if response.status_code != 200:
            raise VectisError(f"Health check failed: {response.text}")
        
        return response.json()
    
    def close(self):
        """Close the HTTP session."""
        self.session.close()
    
    def __enter__(self):
        """Context manager entry."""
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit."""
        self.close()
