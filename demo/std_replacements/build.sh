#!/bin/bash
# -----------------------------------------------------------------------------
# Build script for BioDynaMo ROOT replacement demo
# This script helps users build the demo with different configurations
# -----------------------------------------------------------------------------

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to check for Boost
check_boost() {
    print_info "Checking for Boost..."
    
    # Try to find boost using different methods
    if [ -f "/usr/include/boost/version.hpp" ] || [ -f "/usr/local/include/boost/version.hpp" ] || [ -f "/opt/homebrew/include/boost/version.hpp" ]; then
        print_success "Boost found in system directories"
        return 0
    fi
    
    # Try pkg-config
    if command_exists pkg-config && pkg-config --exists boost; then
        print_success "Boost found via pkg-config"
        return 0
    fi
    
    # Try to compile a simple boost program
    cat > /tmp/boost_test.cpp << 'EOF'
#include <boost/version.hpp>
#include <iostream>
int main() {
    std::cout << "Boost " << BOOST_VERSION / 100000 << "." 
              << BOOST_VERSION / 100 % 1000 << "." 
              << BOOST_VERSION % 100 << std::endl;
    return 0;
}
EOF
    
    if g++ -o /tmp/boost_test /tmp/boost_test.cpp 2>/dev/null; then
        BOOST_VERSION=$(/tmp/boost_test)
        print_success "Boost detected: $BOOST_VERSION"
        rm -f /tmp/boost_test /tmp/boost_test.cpp
        return 0
    fi
    
    rm -f /tmp/boost_test /tmp/boost_test.cpp
    return 1
}

# Function to install Boost
install_boost() {
    print_info "Boost not found. Attempting to install..."
    
    if command_exists apt-get; then
        print_info "Using apt-get (Ubuntu/Debian)..."
        sudo apt-get update
        sudo apt-get install -y libboost-all-dev
    elif command_exists yum; then
        print_info "Using yum (CentOS/RHEL)..."
        sudo yum install -y boost-devel
    elif command_exists dnf; then
        print_info "Using dnf (Fedora)..."
        sudo dnf install -y boost-devel
    elif command_exists brew; then
        print_info "Using brew (macOS)..."
        brew install boost
    elif command_exists pacman; then
        print_info "Using pacman (Arch Linux)..."
        sudo pacman -S boost
    else
        print_error "Cannot automatically install Boost. Please install it manually."
        print_info "Installation commands:"
        print_info "  Ubuntu/Debian: sudo apt-get install libboost-all-dev"
        print_info "  CentOS/RHEL:   sudo yum install boost-devel"
        print_info "  Fedora:        sudo dnf install boost-devel"
        print_info "  macOS:         brew install boost"
        print_info "  Arch Linux:    sudo pacman -S boost"
        return 1
    fi
}

# Main build function
build_demo() {
    local use_std_random=$1
    local use_boost_serialization=$2
    local build_dir=$3
    
    print_info "Building demo with configuration:"
    print_info "  - Standard C++ random: $use_std_random"
    print_info "  - Boost serialization: $use_boost_serialization"
    print_info "  - Build directory: $build_dir"
    
    # Create build directory
    mkdir -p "$build_dir"
    cd "$build_dir"
    
    # Configure with CMake
    cmake_args=()
    if [ "$use_std_random" = "ON" ]; then
        cmake_args+=("-DBDM_USE_STD_RANDOM=ON")
    fi
    if [ "$use_boost_serialization" = "ON" ]; then
        cmake_args+=("-DBDM_USE_BOOST_SERIALIZATION=ON")
    fi
    
    print_info "Running CMake..."
    if ! cmake "${cmake_args[@]}" ..; then
        print_error "CMake configuration failed"
        return 1
    fi
    
    # Build
    print_info "Building..."
    if ! make -j$(nproc 2>/dev/null || echo 4); then
        print_error "Build failed"
        return 1
    fi
    
    print_success "Build completed successfully!"
    
    # Check if executable exists
    if [ -f "./demo_std_replacements" ]; then
        print_info "Demo executable created: ./demo_std_replacements"
        return 0
    else
        print_error "Demo executable not found"
        return 1
    fi
}

# Function to run the demo
run_demo() {
    local build_dir=$1
    
    if [ ! -f "$build_dir/demo_std_replacements" ]; then
        print_error "Demo executable not found in $build_dir"
        return 1
    fi
    
    print_info "Running demo..."
    cd "$build_dir"
    ./demo_std_replacements
}

# Print usage information
show_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --std-random         Enable standard C++ random (default: OFF)"
    echo "  --boost-serial       Enable Boost serialization (default: OFF)"
    echo "  --install-boost      Attempt to install Boost if missing"
    echo "  --build-dir DIR      Specify build directory (default: build)"
    echo "  --run                Run the demo after building"
    echo "  --clean              Clean build directory before building"
    echo "  --help               Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0                                    # Basic build with ROOT"
    echo "  $0 --std-random                      # Use std::random instead of ROOT"
    echo "  $0 --boost-serial --install-boost    # Enable Boost serialization"
    echo "  $0 --std-random --boost-serial --run # Full featured build and run"
}

# Parse command line arguments
use_std_random="OFF"
use_boost_serialization="OFF"
build_dir="build"
install_boost_if_missing=false
run_after_build=false
clean_build=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --std-random)
            use_std_random="ON"
            shift
            ;;
        --boost-serial)
            use_boost_serialization="ON"
            shift
            ;;
        --install-boost)
            install_boost_if_missing=true
            shift
            ;;
        --build-dir)
            build_dir="$2"
            shift 2
            ;;
        --run)
            run_after_build=true
            shift
            ;;
        --clean)
            clean_build=true
            shift
            ;;
        --help)
            show_usage
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            show_usage
            exit 1
            ;;
    esac
done

# Main execution
print_info "BioDynaMo ROOT Replacement Demo Build Script"
print_info "============================================="

# Check prerequisites
if ! command_exists cmake; then
    print_error "CMake not found. Please install CMake."
    exit 1
fi

if ! command_exists make; then
    print_error "Make not found. Please install build tools."
    exit 1
fi

# Check for Boost if needed
if [ "$use_boost_serialization" = "ON" ]; then
    if ! check_boost; then
        if [ "$install_boost_if_missing" = true ]; then
            if ! install_boost; then
                print_error "Failed to install Boost"
                exit 1
            fi
            # Verify installation
            if ! check_boost; then
                print_error "Boost installation verification failed"
                exit 1
            fi
        else
            print_warning "Boost not found. Serialization features will be disabled."
            print_info "Use --install-boost to automatically install Boost."
            use_boost_serialization="OFF"
        fi
    fi
fi

# Clean build directory if requested
if [ "$clean_build" = true ] && [ -d "$build_dir" ]; then
    print_info "Cleaning build directory..."
    rm -rf "$build_dir"
fi

# Build the demo
if ! build_demo "$use_std_random" "$use_boost_serialization" "$build_dir"; then
    print_error "Build failed"
    exit 1
fi

# Run the demo if requested
if [ "$run_after_build" = true ]; then
    if ! run_demo "$build_dir"; then
        print_error "Demo execution failed"
        exit 1
    fi
fi

print_success "Script completed successfully!"
print_info "To run the demo manually: cd $build_dir && ./demo_std_replacements"
