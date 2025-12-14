# Multi-stage build for banking system
FROM gcc:11 AS builder

# Install dependencies
RUN apt-get update && apt-get install -y \
    cmake \
    ninja-build \
    libgtest-dev \
    nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy source code
COPY . .

# Build the application
RUN mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF && \
    make -j$(nproc)

# Runtime stage
FROM ubuntu:20.04

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user
RUN useradd --create-home --shell /bin/bash banking
USER banking

# Set working directory
WORKDIR /home/banking

# Copy binaries from builder stage
COPY --from=builder /app/build/banking_server .
COPY --from=builder /app/build/banking_client .

# Expose port
EXPOSE 8080

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD ./banking_client localhost 8080 || exit 1

# Default command
CMD ["./banking_server", "8080", "4", "3600"]
