FROM debian:bookworm-slim AS build
ENV TZ=Europe/Lisbon
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    curl \
    mosquitto \
    libboost-date-time-dev \
    libmosquittopp-dev \
    libboost-program-options-dev \
    libboost-system-dev \
    libasio-dev \
    libtinyxml2-dev \
    libcrypto++-dev \
    libgeographiclib-dev \
    libgps-dev \
    libssl-dev \
    libnl-3-dev \
    libnl-genl-3-dev \
    zlib1g-dev \
    libcurl4-openssl-dev \
    ca-certificates \
    file \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /tmp
RUN git clone https://github.com/jupp0r/prometheus-cpp.git
WORKDIR /tmp/prometheus-cpp
RUN git checkout a944ec100251019cd44d070bbf2fd22f5139d6d0
RUN git submodule update --init --recursive
RUN cmake -B_build -DCPACK_GENERATOR=DEB -DBUILD_SHARED_LIBS=ON
RUN cmake --build _build --target package --parallel $(nproc)
RUN dpkg -i _build/*.deb

WORKDIR /tmp
RUN git clone https://github.com/eProsima/foonathan_memory_vendor.git
RUN cmake -S foonathan_memory_vendor -B foonathan_memory_vendor/build -DCMAKE_INSTALL_PREFIX=/usr/local -DBUILD_SHARED_LIBS=ON
RUN cmake --build foonathan_memory_vendor/build --target install --parallel $(nproc)

WORKDIR /tmp
RUN git clone https://github.com/eProsima/Fast-CDR.git
RUN cmake -S Fast-CDR -B Fast-CDR/build
RUN cmake --build Fast-CDR/build --target install --parallel $(nproc)

WORKDIR /tmp
RUN git clone https://github.com/eProsima/Fast-DDS.git
WORKDIR /tmp/Fast-DDS
RUN git checkout v2.13.2
RUN cmake -B build
RUN cmake --build build --target install --parallel $(nproc)

# Vanetza
RUN mkdir /vanetza
COPY . /vanetza
WORKDIR /vanetza
RUN rm -f CMakeCache.txt
RUN cmake .
RUN cmake --build . --target socktap -j $(nproc)
RUN cp /vanetza/bin/socktap /usr/local/bin/socktap


FROM debian:bookworm-slim AS final
ENV TZ=Europe/Lisbon
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN apt-get update && apt-get install -y --no-install-recommends \
    mosquitto \
    libboost-date-time-dev \
    libmosquittopp-dev \
    libboost-program-options-dev \
    libboost-system-dev \
    libtinyxml2-dev \
    libcrypto++-dev \
    libgeographiclib-dev \
    libnl-3-dev \
    libnl-genl-3-dev \
    libssl-dev \
    zlib1g \
    iproute2 \
    libgps-dev \
    iptables \
    bridge-utils \
    ebtables \
    tcpdump \
    libcurl4-openssl-dev \
    ca-certificates \
    python3 \
    gpsd \
    gpsd-clients \
    iputils-ping \
    python3-paho-mqtt \
    && rm -rf /var/lib/apt/lists/* \
    && rm -rf /usr/lib/ocaml \
    && rm -rf /usr/bin/ocam* \
    && rm -rf /usr/lib/$(uname -m)-linux-gnu/perl \
    && rm -rf /usr/lib/$(uname -m)-linux-gnu/perl-base \
    && rm -rf /usr/lib/$(uname -m)-linux-gnu/libperl* \
    && rm -rf /usr/share/perl \
    && rm -rf /usr/bin/perl \
    && rm -rf /usr/share/doc/*

WORKDIR /
ENV EMBEDDED_MOSQUITTO_PORT=1883
COPY --from=build /vanetza/bin/socktap /usr/local/bin/socktap
COPY --from=build /vanetza/tools/socktap/config.ini /config.ini
COPY --from=build /vanetza/entrypoint.sh /entrypoint.sh
COPY --from=build /usr/local/lib/libfast* /usr/local/lib
COPY --from=build /usr/local/lib/libfoon* /usr/local/lib
COPY --from=build /tmp/prometheus-cpp/_build/*.deb /deps/
RUN dpkg -i /deps/*.deb
RUN chmod +x /entrypoint.sh
ENTRYPOINT ["/entrypoint.sh"]