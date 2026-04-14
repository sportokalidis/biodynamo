#!/usr/bin/env bash
# =============================================================================
# BioDynaMo Docker Entrypoint
# =============================================================================
# Starts Xvfb (virtual framebuffer) for headless rendering, then sources the
# BioDynaMo environment and executes the given command.
# =============================================================================
set -e

# ---------- Start Xvfb if no DISPLAY is available ----------
# This enables headless ParaView rendering and visualization export.
if [ -z "$DISPLAY" ] || [ "$BDM_HEADLESS" = "1" ]; then
    export DISPLAY=:99
    if ! pgrep -x Xvfb > /dev/null 2>&1; then
        echo "[entrypoint] Starting Xvfb on display ${DISPLAY}..."
        Xvfb ${DISPLAY} -ac -screen 0 2560x1440x24 +extension GLX > /tmp/Xvfb.out 2>&1 &
        # Give Xvfb time to start
        sleep 1
    fi
fi

# ---------- Source PyEnv ----------
export PYENV_ROOT="/home/bdm/.pyenv"
export PATH="$PYENV_ROOT/bin:$PATH"
if command -v pyenv > /dev/null 2>&1; then
    eval "$(pyenv init --path)"
    eval "$(pyenv init -)"
    pyenv shell 3.9.1 2>/dev/null || true
fi

# ---------- Source BioDynaMo ----------
if [ -f /opt/biodynamo/build/bin/thisbdm.sh ]; then
    # shellcheck disable=SC1091
    source /opt/biodynamo/build/bin/thisbdm.sh
fi

# ---------- Execute CMD ----------
exec "$@"
