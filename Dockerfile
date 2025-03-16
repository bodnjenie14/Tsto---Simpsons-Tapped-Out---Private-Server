FROM ubuntu:22.04

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libcurl4-openssl-dev \
    libevent-dev \
    libgoogle-glog-dev \
    libprotobuf-dev \
    libsqlite3-dev \
    libtomcrypt-dev \
    libtommath-dev \
    libzip-dev \
    pkg-config \
    protobuf-compiler \
    zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

# Build the project
RUN mkdir -p build && cd build && \
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_FLAGS="-O3 -Wno-error" \
        -DCMAKE_CXX_STANDARD=20 \
        -DCMAKE_CXX_STANDARD_REQUIRED=ON \
        -DPLATFORM_LINUX=ON \
        -DRAPIDJSON_BUILD_DOC=OFF \
        -DRAPIDJSON_BUILD_EXAMPLES=OFF \
        -DRAPIDJSON_BUILD_TESTS=OFF \
        -DRAPIDJSON_BUILD_THIRDPARTY_GTEST=OFF && \
    cmake --build . --config Release -j$(nproc)

# Set the entry point
ENTRYPOINT ["./build/tsto-server"]
