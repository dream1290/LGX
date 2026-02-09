#!/bin/bash
# Local compatibility testing script
# Tests the LGX Runtime on different distributions using Docker

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "=== LGX Runtime Compatibility Testing ==="
echo ""

# Check if Docker is available
if ! command -v docker &> /dev/null; then
    echo -e "${RED}Error: Docker is not installed${NC}"
    echo "Please install Docker to run compatibility tests"
    exit 1
fi

# Test distributions
DISTRIBUTIONS=(
    "ubuntu:22.04"
    "ubuntu:24.04"
    "fedora:38"
    "fedora:39"
    "archlinux:latest"
)

# Results directory
RESULTS_DIR="${PROJECT_ROOT}/compatibility_results"
mkdir -p "$RESULTS_DIR"

test_distribution() {
    local image=$1
    local dist_name=$(echo $image | tr ':/' '_')
    
    echo -e "${YELLOW}Testing on $image${NC}"
    echo "-----------------------------------"
    
    # Create Dockerfile for this distribution
    cat > "${RESULTS_DIR}/Dockerfile.${dist_name}" << EOF
FROM ${image}

# Install dependencies based on distribution
RUN if [ -f /etc/debian_version ]; then \\
        apt-get update && \\
        apt-get install -y git cmake build-essential \\
            libvulkan-dev libjemalloc-dev python3; \\
    elif [ -f /etc/fedora-release ]; then \\
        dnf install -y git cmake gcc gcc-c++ make \\
            vulkan-headers vulkan-loader-devel jemalloc-devel python3; \\
    elif [ -f /etc/arch-release ]; then \\
        pacman -Syu --noconfirm git cmake base-devel \\
            vulkan-headers vulkan-icd-loader jemalloc python; \\
    fi

WORKDIR /lgx
COPY . /lgx/

# Build and test
RUN cmake -B build-release -DCMAKE_BUILD_TYPE=Release && \\
    cmake --build build-release -j\$(nproc) && \\
    cd build-release && \\
    ctest --output-on-failure || true

CMD ["/bin/bash"]
EOF
    
    # Build and run
    if docker build -t "lgx-test-${dist_name}" -f "${RESULTS_DIR}/Dockerfile.${dist_name}" "$PROJECT_ROOT"; then
        echo -e "${GREEN}✅ Build successful on $image${NC}"
        
        # Run tests and capture output
        docker run --rm "lgx-test-${dist_name}" bash -c "cd build-release && ctest --output-on-failure" \
            > "${RESULTS_DIR}/test_results_${dist_name}.txt" 2>&1 || true
        
        echo -e "${GREEN}✅ Tests completed on $image${NC}"
        echo "   Results: ${RESULTS_DIR}/test_results_${dist_name}.txt"
    else
        echo -e "${RED}❌ Build failed on $image${NC}"
    fi
    
    echo ""
}

# Test each distribution
for dist in "${DISTRIBUTIONS[@]}"; do
    test_distribution "$dist"
done

# Generate summary report
echo "=== Generating Summary Report ==="
cat > "${RESULTS_DIR}/summary.txt" << EOF
LGX Runtime Compatibility Test Summary
======================================

Test Date: $(date)
Tested Distributions: ${#DISTRIBUTIONS[@]}

Results:
EOF

for dist in "${DISTRIBUTIONS[@]}"; do
    dist_name=$(echo $dist | tr ':/' '_')
    result_file="${RESULTS_DIR}/test_results_${dist_name}.txt"
    
    if [ -f "$result_file" ]; then
        passed=$(grep -c "Passed" "$result_file" || echo "0")
        failed=$(grep -c "Failed" "$result_file" || echo "0")
        
        echo "  $dist:" >> "${RESULTS_DIR}/summary.txt"
        echo "    Passed: $passed" >> "${RESULTS_DIR}/summary.txt"
        echo "    Failed: $failed" >> "${RESULTS_DIR}/summary.txt"
    else
        echo "  $dist: Build failed" >> "${RESULTS_DIR}/summary.txt"
    fi
done

cat "${RESULTS_DIR}/summary.txt"

echo ""
echo -e "${GREEN}✅ Compatibility testing complete!${NC}"
echo "Results directory: $RESULTS_DIR"
