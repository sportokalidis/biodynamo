#!/bin/bash
# Wrapper that reminds user to open VNC desktop, then launches real paraview
echo ""
echo "=== ParaView is launching on the VNC Desktop ==="
echo "Switch to the port 6080 browser tab to see the GUI."
echo "If not open yet: Ports tab -> 6080 -> Open in Browser"
echo ""

# Find the real paraview binary (skip this wrapper)
REAL_PARAVIEW=$(which -a paraview | grep -v /usr/local/bin | head -1)
exec "$REAL_PARAVIEW" "$@"
