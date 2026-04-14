#!/usr/bin/env bash
# =============================================================================
# Open an interactive shell inside the BioDynaMo container
# =============================================================================
# Usage:
#   ./docker/scripts/exec_container.sh
# =============================================================================
set -euo pipefail

CONTAINER_NAME="bdm"

if ! docker ps --format '{{.Names}}' | grep -qw "${CONTAINER_NAME}"; then
    echo "Error: Container '${CONTAINER_NAME}' is not running."
    echo "Start it first:  ./docker/scripts/run_container.sh"
    exit 1
fi

docker exec -it "${CONTAINER_NAME}" bash
