#!/bin/bash
#
# Trill Sensor Visualizer Launcher
#
# Starts J-Link connection and pipes RTT output to the visualizer.
#

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VISUALIZER="$SCRIPT_DIR/trill_visualizer.py"

# Check for J-Link tools
if ! command -v JLinkExe &> /dev/null; then
    echo "Error: JLinkExe not found. Install SEGGER J-Link tools."
    exit 1
fi

if ! command -v JLinkRTTClient &> /dev/null; then
    echo "Error: JLinkRTTClient not found. Install SEGGER J-Link tools."
    exit 1
fi

# Kill any existing J-Link processes
pkill -f JLinkExe 2>/dev/null
pkill -f JLinkRTTClient 2>/dev/null
sleep 1

echo "Starting Trill Sensor Visualizer..."
echo ""

# Start JLinkExe in background, connecting and running the target
(
    sleep 1
    echo "device nRF52840_xxAA"
    echo "if SWD"
    echo "speed 4000"
    echo "connect"
    sleep 1
    echo "r"
    echo "g"
    # Keep it running
    while true; do
        sleep 10
    done
) | JLinkExe -NoGui 1 > /dev/null 2>&1 &

JLINK_PID=$!

# Wait for J-Link to connect
echo "Connecting to J-Link..."
sleep 3

# Check if J-Link is still running
if ! kill -0 $JLINK_PID 2>/dev/null; then
    echo "Error: J-Link failed to start. Check connection."
    exit 1
fi

echo "Starting RTT client and visualizer..."
echo "Press Ctrl+C to exit"
echo ""

# Run RTT client and pipe to visualizer
# Trap Ctrl+C to clean up
trap "echo ''; echo 'Shutting down...'; pkill -f JLinkExe; pkill -f JLinkRTTClient; exit 0" INT TERM

JLinkRTTClient 2>/dev/null | python3 "$VISUALIZER"

# Cleanup on exit
pkill -f JLinkExe 2>/dev/null
pkill -f JLinkRTTClient 2>/dev/null
