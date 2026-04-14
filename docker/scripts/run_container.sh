#!/usr/bin/env bash
# =============================================================================
# Create and start a BioDynaMo Docker container
# =============================================================================
# Usage:
#   ./docker/scripts/run_container.sh [--gui]
#
# Options:
#   --gui   Enable X11 forwarding for interactive ParaView GUI.
#           Requires running `xhost +local:docker` on the host first.
#
# Without --gui the container runs in headless mode using Xvfb (default).
# =============================================================================
set -euo pipefail

CONTAINER_NAME="bdm"
IMAGE_NAME="biodynamo:latest"
GUI_MODE=false

if [[ "${1:-}" == "--gui" ]]; then
    GUI_MODE=true
fi

# Stop & remove any previous container with the same name
if docker ps -a --format '{{.Names}}' | grep -qw "${CONTAINER_NAME}"; then
    echo "[run] Removing existing container '${CONTAINER_NAME}'..."
    docker rm -f "${CONTAINER_NAME}" > /dev/null 2>&1 || true
fi

echo "============================================="
echo " Starting BioDynaMo container"
echo " Container: ${CONTAINER_NAME}"
echo " Image:     ${IMAGE_NAME}"
echo " Mode:      $(if $GUI_MODE; then echo 'GUI (X11)'; else echo 'Headless (Xvfb)'; fi)"
echo "============================================="

DOCKER_RUN_ARGS=(
    --name "${CONTAINER_NAME}"
    --hostname bdm-docker
    --cap-add=SYS_PTRACE
    --security-opt seccomp=unconfined
    -dit
)

if $GUI_MODE; then
    # GUI mode: forward the host X11 display
    DOCKER_RUN_ARGS+=(
        --net=host
        --env "DISPLAY=${DISPLAY:-:0}"
        --volume /tmp/.X11-unix:/tmp/.X11-unix:rw
    )
    # GPU passthrough (if /dev/dri exists)
    if [ -d /dev/dri ]; then
        DOCKER_RUN_ARGS+=(--device=/dev/dri:/dev/dri)
    fi
else
    # Headless mode: Xvfb will be started by the entrypoint
    DOCKER_RUN_ARGS+=(
        --env "BDM_HEADLESS=1"
    )
fi

docker run "${DOCKER_RUN_ARGS[@]}" "${IMAGE_NAME}" /bin/bash

echo ""
echo "Container '${CONTAINER_NAME}' is running."
if $GUI_MODE; then
    echo "ParaView GUI mode is enabled. 'bdm view' should open visible windows."
else
    echo "Headless mode is enabled (Xvfb). 'bdm view' GUI windows are not visible."
    echo "Use './docker/scripts/run_container.sh --gui' for interactive ParaView."
fi
echo ""
echo "Enter the container:"
echo "  docker exec -it ${CONTAINER_NAME} bash"
echo ""
echo "Or run a quick smoke test:"
echo "  ./docker/scripts/test_biodynamo.sh"
