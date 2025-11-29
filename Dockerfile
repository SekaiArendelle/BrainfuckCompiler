# BrainfuckCompiler Dockerfile
# Multi-stage build for stable and efficient compilation

# Build stage - using latest stable LLVM
FROM ubuntu:24.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    wget \
    gnupg \
    lsb-release \
    software-properties-common \
    build-essential \
    cmake \
    ninja-build \
    git \
    && rm -rf /var/lib/apt/lists/*

# Add LLVM official repository for the latest stable version
RUN wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add - && \
    add-apt-repository "deb http://apt.llvm.org/$(lsb_release -cs)/ llvm-toolchain-$(lsb_release -cs)-21 main" && \
    apt-get update

# Install LLVM 21 development packages (latest stable as of 2025)
RUN apt-get install -y \
    llvm-21-dev \
    clang-21 \
    libclang-21-dev \
    lldb-21 \
    lld-21 \
    && rm -rf /var/lib/apt/lists/*

# Set LLVM 21 as the default
RUN update-alternatives --install /usr/bin/llvm-config llvm-config /usr/bin/llvm-config-21 100 && \
    update-alternatives --install /usr/bin/clang clang /usr/bin/clang-21 100 && \
    update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-21 100

# Set working directory
WORKDIR /build

# Copy source code
COPY . .

# Create build directory and configure
RUN mkdir -p build && cd build && \
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER=clang \
        -DCMAKE_CXX_COMPILER=clang++ \
        -DCMAKE_CXX_STANDARD=17 \
        -DLLVM_CONFIG_PATH=/usr/bin/llvm-config-21 \
        -G Ninja

# Build the project
RUN cd build && ninja -j$(nproc)

# Runtime stage - minimal image
FROM ubuntu:24.04

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    llvm-21 \
    libllvm21 \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user
RUN useradd -m -s /bin/bash bfc

# Copy built binary and examples
COPY --from=builder /build/build/bin/bfc /usr/local/bin/
COPY --from=builder /build/examples /home/bfc/examples

# Set ownership
RUN chown -R bfc:bfc /home/bfc

# Switch to non-root user
USER bfc
WORKDIR /home/bfc

# Set environment variables
ENV PATH="/usr/local/bin:${PATH}"
ENV LLVM_VERSION="21"

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD bfc --help || exit 1

# Default command
CMD ["bfc", "--help"]

# Metadata
LABEL maintainer="BrainfuckCompiler Project"
LABEL description="Brainfuck compiler with LLVM 21 backend"
LABEL version="1.0.0"
LABEL llvm.version="21"