#!/usr/bin/env bash
# =============================================================================
# Create and start a BioDynaMo Docker container
# =============================================================================
# Usage:
#   ./docker/scripts/run_container.sh [--gui] [--image IMAGE]
#
# Options:
#   --gui   Enable X11 forwarding for interactive ParaView GUI.
#           Requires running `xhost +local:docker` on the host first.
#   --image Container image to run (e.g. ghcr.io/org/biodynamo:latest)
#
# Without --gui the container runs in headless mode using Xvfb (default).
# =============================================================================
set -euo pipefail

CONTAINER_NAME="bdm"
IMAGE_NAME="${BDM_IMAGE:-biodynamo:latest}"
GUI_MODE=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --gui)
            GUI_MODE=true
            shift
            ;;
        --image)
            if [[ $# -lt 2 ]]; then
                echo "Error: --image requires a value"
                exit 1
            fi
            IMAGE_NAME="$2"
            shift 2
            ;;
        *)
            echo "Error: Unknown option '$1'"
            echo "Usage: ./docker/scripts/run_container.sh [--gui] [--image IMAGE]"
            exit 1
            ;;
    esac
done

if ! docker image inspect "${IMAGE_NAME}" > /dev/null 2>&1; then
    echo "Error: Docker image '${IMAGE_NAME}' was not found locally."
    echo ""
    echo "Either build it:"
    echo "  ./docker/scripts/build_image.sh"
    echo ""
    echo "Or pull a prebuilt image:"
    echo "  ./docker/scripts/pull_image.sh --image ${IMAGE_NAME%:*} --tag ${IMAGE_NAME##*:}"
    exit 1
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
