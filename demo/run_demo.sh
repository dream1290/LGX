#!/bin/bash
#
# LGX Runtime Demo Runner
# 
# This script builds and runs the game simulation demo, then analyzes the results.
#

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=================================${NC}"
echo -e "${BLUE}LGX Runtime Demo Runner${NC}"
echo -e "${BLUE}=================================${NC}"
echo

# Check if we're in the demo directory
if [ ! -f "demo_game_simulation.c" ]; then
    echo -e "${RED}Error: Please run this script from the demo/ directory${NC}"
    exit 1
fi

# Go to project root
cd ..

# Check if build directory exists
if [ ! -d "build" ]; then
    echo -e "${YELLOW}Build directory not found. Creating...${NC}"
    mkdir build
fi

cd build

# Configure if needed
if [ ! -f "Makefile" ]; then
    echo -e "${YELLOW}Configuring project...${NC}"
    cmake -DCMAKE_BUILD_TYPE=Release ..
fi

# Build the demo
echo -e "${GREEN}Building demo...${NC}"
make demo_game_simulation -j$(nproc)

if [ $? -ne 0 ]; then
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi

echo -e "${GREEN}Build successful!${NC}"
echo

# Run the demo
echo -e "${GREEN}Running demo...${NC}"
echo

# Set library path
export LD_LIBRARY_PATH=$(pwd):$LD_LIBRARY_PATH

./demo_game_simulation

if [ $? -ne 0 ]; then
    echo -e "${RED}Demo execution failed!${NC}"
    exit 1
fi

echo
echo -e "${GREEN}Demo completed successfully!${NC}"
echo

# Check if results file exists
if [ ! -f "demo_results.csv" ]; then
    echo -e "${RED}Error: Results file not generated${NC}"
    exit 1
fi

# Analyze results
echo -e "${BLUE}Analyzing results...${NC}"
echo

if command -v python3 &> /dev/null; then
    python3 ../demo/analyze_results.py demo_results.csv
else
    echo -e "${YELLOW}Python3 not found. Skipping analysis.${NC}"
    echo -e "${YELLOW}Install Python3 to generate plots and statistics.${NC}"
fi

echo
echo -e "${GREEN}All done!${NC}"
echo -e "Results saved to: ${BLUE}$(pwd)/demo_results.csv${NC}"

if [ -d "plots" ]; then
    echo -e "Plots saved to: ${BLUE}$(pwd)/plots/${NC}"
fi

echo
echo -e "${YELLOW}To view the CSV data:${NC}"
echo -e "  cat demo_results.csv"
echo
echo -e "${YELLOW}To generate plots manually:${NC}"
echo -e "  python3 ../demo/analyze_results.py demo_results.csv"
echo
