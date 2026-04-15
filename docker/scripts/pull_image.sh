#!/usr/bin/env bash
# =============================================================================
# Pull a prebuilt BioDynaMo image from a container registry
# =============================================================================
# Usage:
#   ./docker/scripts/pull_image.sh [--image IMAGE] [--tag TAG]
#
# Examples:
#   ./docker/scripts/pull_image.sh
#   ./docker/scripts/pull_image.sh --image ghcr.io/sportokalidis/biodynamo --tag v1.0.0
# =============================================================================
set -euo pipefail

IMAGE_REPO="${BDM_IMAGE_REPO:-ghcr.io/sportokalidis/biodynamo}"
IMAGE_TAG="${BDM_IMAGE_TAG:-latest}"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --image)
            if [[ $# -lt 2 ]]; then
                echo "Error: --image requires a value"
                exit 1
            fi
            IMAGE_REPO="$2"
            shift 2
            ;;
        --tag)
            if [[ $# -lt 2 ]]; then
                echo "Error: --tag requires a value"
                exit 1
            fi
            IMAGE_TAG="$2"
            shift 2
            ;;
        *)
            echo "Error: Unknown option '$1'"
            echo "Usage: ./docker/scripts/pull_image.sh [--image IMAGE] [--tag TAG]"
            exit 1
            ;;
    esac
done

FULL_IMAGE="${IMAGE_REPO}:${IMAGE_TAG}"

echo "============================================="
echo " Pulling BioDynaMo image"
echo " Image: ${FULL_IMAGE}"
echo "============================================="

docker pull "${FULL_IMAGE}"

echo ""
echo "Pulled successfully."
echo "Run with:"
echo "  ./docker/scripts/run_container.sh --image ${FULL_IMAGE}"
