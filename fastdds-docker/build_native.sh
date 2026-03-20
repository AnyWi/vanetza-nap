#!/bin/bash
# build_native.sh - Build directly on Ubuntu without Docker
# Requirements: FastDDS installed, optionally Vanetza installed

set -e

echo "=== Native Build Script for Vanetza DDS Test ==="

# Check FastDDS
if ! pkg-config --exists fastrtps 2>/dev/null && ! find /usr /usr/local -name "fastrtps-config.cmake" 2>/dev/null | grep -q .; then
    echo "[!] FastDDS not found. Installing..."
    
    # Install dependencies
    sudo apt-get update
    sudo apt-get install -y build-essential cmake git libboost-all-dev libssl-dev \
        libasio-dev libtinyxml2-dev

    # Fast-CDR
    git clone --depth 1 --branch v2.10.1 https://github.com/eProsima/Fast-CDR.git /tmp/fast-cdr
    cd /tmp/fast-cdr \
    mkdir -p build && cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    make -j$(nproc) && sudo make install && sudo ldconfig
    cd -

    # FastDDS
    git clone --depth 1 --branch v2.10.2 https://github.com/eProsima/Fast-DDS.git /tmp/fast-dds
    cd /tmp/fast-dds \
    mkdir -p build && cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release -DTHIRDPARTY=ON
    make -j$(nproc) && sudo make install && sudo ldconfig
    cd -
    echo "[✓] FastDDS installed"
else
    echo "[✓] FastDDS found"
fi

# Check Vanetza
VANETZA_FOUND=0
if find /usr /usr/local -name "vanetza-config.cmake" 2>/dev/null | grep -q .; then
    VANETZA_FOUND=1
    echo "[✓] Vanetza found"
else
    echo "[!] Vanetza not found - will build FastDDS-only examples"
    echo "    To install Vanetza: git clone https://github.com/riebl/vanetza.git"
    echo "    Then: cd vanetza && mkdir build && cd build && cmake .. && make && sudo make install"
fi

# Build project
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
cd ..

echo ""
echo "=== Build complete! ==="
echo ""
echo "Run in separate terminals:"
echo "  Terminal 1:  ./build/dds_subscriber"
echo "  Terminal 2:  ./build/dds_publisher [interval_ms] [max_msgs]"
echo ""
if [ $VANETZA_FOUND -eq 1 ]; then
echo "Vanetza CAM test:"
echo "  Terminal 1:  ./build/vanetza_cam_subscriber"
echo "  Terminal 2:  ./build/vanetza_cam_publisher"
fi
