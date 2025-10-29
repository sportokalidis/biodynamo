#!/bin/bash
# Environment setup for BioDynaMo on macOS
# This script sets up necessary environment variables for keg-only Homebrew packages
# Source this file: source util/installation/osx/setup_env.sh

# Check if we're on macOS
if [[ "$OSTYPE" != "darwin"* ]]; then
    echo "This script is for macOS only"
    return 1
fi

# Check if Homebrew is installed
if command -v brew >/dev/null 2>&1; then
    BREW_PREFIX=$(brew --prefix)
else
    echo "Homebrew not found. Please install Homebrew first."
    return 1
fi

# Set up environment variables for keg-only packages
export LDFLAGS="-L${BREW_PREFIX}/opt/zlib/lib ${LDFLAGS}"
export CPPFLAGS="-I${BREW_PREFIX}/opt/zlib/include ${CPPFLAGS}"

# Add zlib library path to DYLD_LIBRARY_PATH for runtime linking
export DYLD_LIBRARY_PATH="${BREW_PREFIX}/opt/zlib/lib:${DYLD_LIBRARY_PATH}"

# Also add zstd and ncurses for completeness
export LDFLAGS="-L${BREW_PREFIX}/opt/zstd/lib -L${BREW_PREFIX}/opt/ncurses/lib ${LDFLAGS}"
export CPPFLAGS="-I${BREW_PREFIX}/opt/zstd/include -I${BREW_PREFIX}/opt/ncurses/include ${CPPFLAGS}"
export DYLD_LIBRARY_PATH="${BREW_PREFIX}/opt/zstd/lib:${BREW_PREFIX}/opt/ncurses/lib:${DYLD_LIBRARY_PATH}"

# Clean up DYLD_LIBRARY_PATH (remove empty entries and duplicates)
export DYLD_LIBRARY_PATH=$(echo $DYLD_LIBRARY_PATH | tr ':' '\n' | grep -v '^$' | sort -u | tr '\n' ':' | sed 's/:$//')

echo "Environment variables set up for BioDynaMo on macOS:"
echo "LDFLAGS=${LDFLAGS}"
echo "CPPFLAGS=${CPPFLAGS}"
echo "DYLD_LIBRARY_PATH=${DYLD_LIBRARY_PATH}"