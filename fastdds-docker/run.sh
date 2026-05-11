#!/bin/bash
# run.sh - Start publisher or subscriber based on argument

ROLE="${1:-${ROLE:-publisher}}"
BUILD_DIR="/app/build"

case "$ROLE" in
    publisher)
        echo "[run.sh] Starting DDS Publisher..."
        exec "$BUILD_DIR/dds_publisher" cam enc 2000 # cam|cpm json|enc interval
        ;;
    subscriber)
        echo "[run.sh] Starting DDS Subscriber..."
        exec "$BUILD_DIR/dds_subscriber"
        ;;
    cam_publisher)
        echo "[run.sh] Starting Vanetza CAM Publisher..."
        exec "$BUILD_DIR/vanetza_cam_publisher"
        ;;
    cam_subscriber)
        echo "[run.sh] Starting Vanetza CAM Subscriber..."
        exec "$BUILD_DIR/vanetza_cam_subscriber"
        ;;
    *)
        echo "Usage: run.sh [publisher|subscriber|cam_publisher|cam_subscriber]"
        exit 1
        ;;
esac
