FROM ubuntu:latest

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y cmake g++ make curl git && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . /app

RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y

ENV PATH="/root/.cargo/bin:${PATH}"

RUN rustup target add aarch64-unknown-linux-gnu

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
    -DLIBMUSICTOK_BUILD_TESTS=ON -DLIBMUSICTOK_BUILD_BENCHMARKS=ON

RUN cmake --build build --config Release

CMD ["/bin/sh", "-c", "./build/LibMusicTokTest && ./build/LibMusicTokBenchmark --benchmark_time_unit=ms"]
