#!/bin/bash
# Copyright 2024 DPDK Demo
# Test script for DPDK filter demo

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
DPDK_PORT=0
FORWARD_IF="tun0"
KNI_IF="vEth0"
TEST_DST_IP="192.168.182.128"
TEST_DST_PORT="7777"
TEST_SRC_IP="192.168.182.1"
TEST_SRC_PORT="8752"

echo -e "${GREEN}DPDK Filter Demo Test Script${NC}"
echo "================================"

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
    echo -e "${RED}Error: Please run as root${NC}"
    exit 1
fi

# Check if DPDK is installed
if ! command -v dpdk-devbind.py &> /dev/null; then
    echo -e "${YELLOW}Warning: dpdk-devbind.py not found. Make sure DPDK is installed.${NC}"
fi

# Check hugepages
HUGEPAGES=$(cat /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages)
if [ "$HUGEPAGES" -lt 512 ]; then
    echo -e "${YELLOW}Warning: Only $HUGEPAGES hugepages allocated. Recommended: 512+${NC}"
    echo "To allocate more: echo 1024 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages"
fi

# Check KNI module
if ! lsmod | grep -q rte_kni; then
    echo -e "${YELLOW}Warning: rte_kni module not loaded${NC}"
    echo "To load: sudo modprobe rte_kni"
fi

# Create TUN device if it doesn't exist
if ! ip link show "$FORWARD_IF" &> /dev/null; then
    echo "Creating TUN device: $FORWARD_IF"
    ip tuntap add mode tun name "$FORWARD_IF"
    ip addr add 10.0.0.1/24 dev "$FORWARD_IF"
    ip link set "$FORWARD_IF" up
    echo -e "${GREEN}Created TUN device: $FORWARD_IF${NC}"
fi

# Check if program exists
if [ ! -f "./dpdk_filter" ]; then
    echo -e "${RED}Error: dpdk_filter not found. Please compile first with 'make'${NC}"
    exit 1
fi

echo ""
echo "Starting DPDK filter program..."
echo "Configuration:"
echo "  DPDK Port: $DPDK_PORT"
echo "  Forward Interface: $FORWARD_IF"
echo "  KNI Interface: $KNI_IF"
echo "  Filter Rules:"
echo "    - Destination: $TEST_DST_IP:$TEST_DST_PORT"
echo "    - Source: $TEST_SRC_IP:$TEST_SRC_PORT"
echo ""
echo "Press Ctrl+C to stop"
echo ""

# Run the program
./dpdk_filter -l 0-3 -n 4 -- \
    -i "$DPDK_PORT" \
    -f "$FORWARD_IF" \
    -d "$TEST_DST_IP:$TEST_DST_PORT" \
    -s "$TEST_SRC_IP:$TEST_SRC_PORT" \
    -k "$KNI_IF"

echo ""
echo -e "${GREEN}Program stopped${NC}"
