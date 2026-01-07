"""
Vector utilities for Vectis Python SDK.
"""

import numpy as np
from typing import List, Union


class Vector:
    """
    Vector representation for embeddings.
    
    Provides utilities for vector operations and conversions.
    """
    
    def __init__(self, data: Union[List[float], np.ndarray]):
        """
        Initialize a vector.
        
        Args:
            data: List of floats or numpy array
        """
        if isinstance(data, np.ndarray):
            self.data = data.flatten().astype(np.float32)
        else:
            self.data = np.array(data, dtype=np.float32)
    
    def to_list(self) -> List[float]:
        """Convert vector to Python list."""
        return self.data.tolist()
    
    def to_numpy(self) -> np.ndarray:
        """Convert vector to numpy array."""
        return self.data
    
    def normalize(self) -> 'Vector':
        """
        Normalize the vector to unit length (L2 norm = 1).
        
        Returns:
            Normalized vector
        """
        norm = np.linalg.norm(self.data)
        if norm > 0:
            return Vector(self.data / norm)
        return Vector(self.data)
    
    def cosine_similarity(self, other: 'Vector') -> float:
        """
        Compute cosine similarity with another vector.
        
        Args:
            other: Another Vector instance
            
        Returns:
            Cosine similarity (-1 to 1, 1 = identical direction)
        """
        dot_product = np.dot(self.data, other.data)
        norm_self = np.linalg.norm(self.data)
        norm_other = np.linalg.norm(other.data)
        
        if norm_self == 0 or norm_other == 0:
            return 0.0
        
        return float(dot_product / (norm_self * norm_other))
    
    def euclidean_distance(self, other: 'Vector') -> float:
        """
        Compute Euclidean distance to another vector.
        
        Args:
            other: Another Vector instance
            
        Returns:
            Euclidean distance (L2 distance)
        """
        return float(np.linalg.norm(self.data - other.data))
    
    def dot_product(self, other: 'Vector') -> float:
        """
        Compute dot product with another vector.
        
        Args:
            other: Another Vector instance
            
        Returns:
            Dot product
        """
        return float(np.dot(self.data, other.data))
    
    def __len__(self) -> int:
        """Get vector dimension."""
        return len(self.data)
    
    def __repr__(self) -> str:
        """String representation."""
        return f"Vector(dim={len(self)}, data={self.data[:5]}...)"
    
    def __getitem__(self, index):
        """Get element at index."""
        return self.data[index]
    
    def __setitem__(self, index, value):
        """Set element at index."""
        self.data[index] = value
