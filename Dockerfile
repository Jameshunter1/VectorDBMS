# Lightweight Linux Dockerfile for Vectis Database
FROM ubuntu:22.04 AS builder

# Install dependencies (including CMake 3.24+ from Kitware's repository)
RUN apt-get update && apt-get install -y \
    build-essential \
    git \
    ninja-build \
    wget \
    && rm -rf /var/lib/apt/lists/*

# Install CMake 3.24+ from official Kitware repository
RUN wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - > /usr/share/keyrings/kitware-archive-keyring.gpg && \
    echo 'deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ jammy main' > /etc/apt/sources.list.d/kitware.list && \
    apt-get update && \
    apt-get install -y cmake && \
    rm -rf /var/lib/apt/lists/*

# Copy source code
WORKDIR /build
COPY . .

# Build the project
RUN cmake -B build -S src -DCMAKE_BUILD_TYPE=Release -GNinja && \
    cmake --build build -j$(nproc)

# Runtime image
FROM ubuntu:22.04

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libstdc++6 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /vectis

# Copy built executables from Release build
COPY --from=builder /build/build/Release/dbweb* ./dbweb
COPY --from=builder /build/build/Release/dbcli* ./dbcli
# Note: core_engine is a static library, not needed at runtime

# Create data directory
RUN mkdir -p /vectis/data

# Expose HTTP port
EXPOSE 8080

# Health check
HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
    CMD wget -q --spider http://localhost:8080/api/health || exit 1

# Set environment variables
ENV VECTIS_DATA_DIR=/vectis/data
ENV VECTIS_PORT=8080
ENV VECTIS_HOST=0.0.0.0

# Non-root user for security
RUN useradd -r -u 1001 -g root vectis && \
    chown -R vectis:root /vectis && \
    chmod -R 755 /vectis

USER vectis

# Run the web server
CMD ["/vectis/dbweb", "/vectis/data", "8080"]
