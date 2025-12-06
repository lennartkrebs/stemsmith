# syntax=docker/dockerfile:1

ARG BASE_IMAGE=ubuntu:24.04

FROM ${BASE_IMAGE} AS build

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        git \
        curl \
        ca-certificates \
        pkg-config \
        libcurl4-openssl-dev \
        libomp-dev \
        libopenblas-dev \
        libsndfile1-dev \
        libsamplerate0-dev && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -S . -B build \
        -DCMAKE_BUILD_TYPE=Release \
        -DENABLE_OPENMP=ON \
        -DSTEMSMITH_BUILD_EXAMPLES=OFF && \
    cmake --build build --target stemsmithd

FROM ${BASE_IMAGE} AS runtime

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        ca-certificates \
        libcurl4 \
        libomp5 \
        libopenblas0 \
        libsndfile1 \
        libsamplerate0 && \
    rm -rf /var/lib/apt/lists/*

ENV STEMSMITH_HOME=/root/.stemsmith
WORKDIR /app

COPY --from=build /src/build/stemsmithd /usr/local/bin/stemsmithd
# Preserve data files at the path baked into the binary (set during CMake configure)
# The CMakeLists sets STEMSMITH_DATA_DIR to the source-tree data path (/src/data in the build stage).
COPY data /src/data

EXPOSE 8345
VOLUME ["/root/.stemsmith"]

ENTRYPOINT ["stemsmithd"]
