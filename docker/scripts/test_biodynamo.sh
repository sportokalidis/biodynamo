#!/usr/bin/env bash
# =============================================================================
# Smoke-test BioDynaMo inside the Docker container
# =============================================================================
# Usage:
#   ./docker/scripts/test_biodynamo.sh
#
# Runs inside the "bdm" container and validates:
#   1. BioDynaMo environment is sourced
#   2. `bdm` CLI is available
#   3. `bdm config` prints configuration
#   4. A demo simulation can be created and built
#   5. The demo simulation runs successfully
# =============================================================================
set -euo pipefail

CONTAINER_NAME="bdm"
PASS=0
FAIL=0

run_in_container() {
      local cmd="$1"
      docker exec "${CONTAINER_NAME}" bash -lc "
            export PYENV_ROOT=\"/home/bdm/.pyenv\"
            export PATH=\"\$PYENV_ROOT/bin:\$PATH\"
            if command -v pyenv > /dev/null 2>&1; then
                  eval \"\$(pyenv init --path)\"
                  eval \"\$(pyenv init -)\"
                  pyenv shell 3.9.1 2>/dev/null || true
            fi
            if [ -f /opt/biodynamo/build/bin/thisbdm.sh ]; then
                  source /opt/biodynamo/build/bin/thisbdm.sh
            fi
            if [ -z \"\$DISPLAY\" ]; then
                  export DISPLAY=:99
            fi
            ${cmd}
      "
}

check() {
    local label="$1"
    local cmd="$2"
    printf "  %-50s " "${label}..."
    if run_in_container "$cmd" > /dev/null 2>&1; then
        echo "PASS"
            PASS=$((PASS + 1))
    else
        echo "FAIL"
            FAIL=$((FAIL + 1))
    fi
}

echo "============================================="
echo " BioDynaMo Docker Smoke Tests"
echo "============================================="
echo ""

# Verify container is running
if ! docker ps --format '{{.Names}}' | grep -qw "${CONTAINER_NAME}"; then
    echo "Error: Container '${CONTAINER_NAME}' is not running."
    echo "Start it first:  ./docker/scripts/run_container.sh"
    exit 1
fi

echo "Test suite:"
echo ""

check "BioDynaMo env sourced (BDMSYS set)" \
      'test -n "$BDMSYS"'

check "bdm CLI available" \
      'command -v bdm'

check "bdm config runs" \
      'bdm config'

check "cmake available" \
      'cmake --version'

check "Python 3.9 available" \
      'python --version 2>&1 | grep -q "3.9"'

check "ROOT available" \
      'root-config --version'

check "Xvfb is running" \
      'pgrep -x Xvfb'

# Create and run a demo project
check "Create demo project (tumor_concept)" \
      'cd /tmp && rm -rf bdm_test_demo && mkdir bdm_test_demo && cd bdm_test_demo && bdm demo tumor_concept'

check "Build demo project" \
      'cd /tmp/bdm_test_demo/tumor_concept && bdm build'

check "Run demo project" \
      'cd /tmp/bdm_test_demo/tumor_concept && bdm run'

# Cleanup test artifacts
run_in_container 'rm -rf /tmp/bdm_test_demo' > /dev/null 2>&1 || true

echo ""
echo "============================================="
echo " Results: ${PASS} passed, ${FAIL} failed"
echo "============================================="

if [ "${FAIL}" -gt 0 ]; then
    echo ""
    echo "Some tests failed. Debug with:"
    echo "  docker exec -it ${CONTAINER_NAME} bash"
    exit 1
fi

echo ""
echo "All tests passed — BioDynaMo is ready to use!"
