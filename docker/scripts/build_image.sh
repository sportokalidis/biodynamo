#!/usr/bin/env bash
# =============================================================================
# Build the BioDynaMo Docker image
# =============================================================================
# Usage:
#   ./docker/scripts/build_image.sh [--no-cache]
#
# The image is tagged as biodynamo:latest.
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

IMAGE_NAME="biodynamo:latest"
DOCKER_ARGS=""

if [[ "${1:-}" == "--no-cache" ]]; then
    DOCKER_ARGS="--no-cache"
    echo "[build] Building without Docker cache..."
fi

echo "============================================="
echo " Building BioDynaMo Docker image"
echo " Image:   ${IMAGE_NAME}"
echo " Context: ${REPO_ROOT}"
echo "============================================="
echo ""

docker build \
    ${DOCKER_ARGS} \
    --build-arg HOST_UID="$(id -u)" \
    --build-arg HOST_GID="$(id -g)" \
    -t "${IMAGE_NAME}" \
    -f "${REPO_ROOT}/docker/Dockerfile" \
    "${REPO_ROOT}"

echo ""
echo "============================================="
echo " Image built successfully: ${IMAGE_NAME}"
echo "============================================="
echo ""
echo "Next step — create a container:"
echo "  ./docker/scripts/run_container.sh"
