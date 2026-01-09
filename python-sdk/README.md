# Vectis Python SDK
Official Python client for Vectis — a high-performance page-oriented vector database.

# Installation

`pip install vectis`

Or install from source:

`git clone https://github.com/yourusername/VectorDBMS`
`cd VectorDBMS/python-sdk`
`pip install -e .`

# Quick Start

`from vectis import VectisClient`
`client = VectisClient("http://localhost:8080")`
`client.put("user:123", "John Doe")`
`print(client.get("user:123"))`

# Documentation
For server setup, deployment, and API details, see the main [DOCUMENTATION.md](../docs/DOCUMENTATION.md).

# License

MIT License — see LICENSE file for details.