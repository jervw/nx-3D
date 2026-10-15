FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    libboost-json-dev \
    libssl-dev \
    libbullet-dev \
    libglm-dev \
    python3-pip \
    python3-venv \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

RUN python3 -m venv /opt/cmake \
    && /opt/cmake/bin/pip install --no-cache-dir 'cmake>=3.30,<5'
ENV PATH="/opt/cmake/bin:${PATH}"

WORKDIR /app
COPY . .

# Use the checked-out Nexilis source and the distribution's Boost package.
RUN test -f nexilis/nexilis/CMakeLists.txt \
    && CI=1 cmake -S . -B /tmp/nx3d-build \
        -DCMAKE_BUILD_TYPE=Release \
        -DNX3D_BUILD_CLIENT=OFF \
        -DBUILD_TESTING=OFF \
    && cmake --build /tmp/nx3d-build --target nx-3D-server --parallel 2 \
    && cp /tmp/nx3d-build/server/nx-3D-server /app/nx-3D-server \
    && cp /tmp/nx3d-build/nexilis/libnexilis.so* /app/ \
    && rm -rf /tmp/nx3d-build

ENV LD_LIBRARY_PATH=/app
CMD ["/app/nx-3D-server"]
