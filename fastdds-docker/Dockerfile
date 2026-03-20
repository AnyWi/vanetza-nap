FROM debian:bullseye-slim AS build

ENV TZ=Europe/Lisbon
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

# Add bullseye-backports for a newer CMake
RUN printf "deb http://archive.debian.org/debian-archive/debian bullseye-backports main non-free\ndeb-src http://archive.debian.org/debian-archive/debian bullseye-backports main non-free\n" \
    > /etc/apt/sources.list.d/backports.list

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    python3 \
    cmake/bullseye-backports \
    cmake-data/bullseye-backports \
    git \
    curl \
    libboost-date-time-dev \
    libboost-program-options-dev \
    libboost-system-dev \
    libasio-dev \
    libtinyxml2-dev \
    libcrypto++-dev \
    libgeographic-dev \
    libssl-dev \
    zlib1g-dev \
    ca-certificates \
    file \
    && rm -rf /var/lib/apt/lists/*

# ── foonathan_memory (FastDDS dependency) ────────────────────────────────────
WORKDIR /tmp
RUN git clone https://github.com/eProsima/foonathan_memory_vendor.git
RUN mkdir /tmp/foonathan_memory_vendor/build
WORKDIR /tmp/foonathan_memory_vendor/build
RUN cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local/ -DBUILD_SHARED_LIBS=ON
RUN cmake --build . --target install --parallel $(nproc)

# ── Fast-CDR ─────────────────────────────────────────────────────────────────
WORKDIR /tmp
RUN git clone https://github.com/eProsima/Fast-CDR.git
RUN mkdir /tmp/Fast-CDR/build
WORKDIR /tmp/Fast-CDR/build
RUN cmake ..
RUN cmake --build . --target install --parallel $(nproc)

# ── Fast-DDS v2.13.2 ─────────────────────────────────────────────────────────
WORKDIR /tmp
RUN git clone https://github.com/eProsima/Fast-DDS.git
WORKDIR /tmp/Fast-DDS/
RUN git checkout v2.13.2
RUN mkdir /tmp/Fast-DDS/build
WORKDIR /tmp/Fast-DDS/build
RUN cmake ..
RUN cmake --build . --target install --parallel $(nproc)
RUN ldconfig

# ── Build our test app ───────────────────────────────────────────────────────
WORKDIR /app
COPY src/ /app/src/
COPY CMakeLists.txt /app/
COPY run.sh /app/

RUN chmod +x /app/run.sh
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release
RUN cmake --build build --parallel $(nproc)

RUN mkdir -p /app/logs

CMD ["/app/run.sh"]