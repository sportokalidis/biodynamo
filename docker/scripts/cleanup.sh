#!/usr/bin/env bash
# =============================================================================
# Cleanup BioDynaMo Docker resources
# =============================================================================
# Usage:
#   ./docker/scripts/cleanup.sh           — stop & remove the container
#   ./docker/scripts/cleanup.sh --all     — also remove the image
# =============================================================================
set -euo pipefail

CONTAINER_NAME="bdm"
IMAGE_NAME="biodynamo:latest"

echo "Stopping and removing container '${CONTAINER_NAME}'..."
docker rm -f "${CONTAINER_NAME}" 2>/dev/null && echo "  Container removed." || echo "  No container found."

if [[ "${1:-}" == "--all" ]]; then
    echo "Removing image '${IMAGE_NAME}'..."
    docker rmi "${IMAGE_NAME}" 2>/dev/null && echo "  Image removed." || echo "  No image found."

    echo "Pruning dangling images..."
    docker image prune -f
fi

echo "Done."
