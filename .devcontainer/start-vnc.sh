#!/bin/bash
export DISPLAY=:1
export XDG_RUNTIME_DIR=/tmp/runtime-root
mkdir -p "$XDG_RUNTIME_DIR" && chmod 700 "$XDG_RUNTIME_DIR"

# Kill any existing processes
killall Xvfb x11vnc fluxbox websockify 2>/dev/null || true
rm -f /tmp/.X1-lock /tmp/.X11-unix/X1 2>/dev/null
rm -f /tmp/xvfb.pid /tmp/x11vnc.pid /tmp/fluxbox.pid /tmp/websockify.pid
sleep 1

# Start Xvfb virtual framebuffer
start-stop-daemon --start --background --pidfile /tmp/xvfb.pid --make-pidfile \
  --exec /usr/bin/Xvfb -- :1 -screen 0 1920x1080x24
sleep 2

# Start window manager
start-stop-daemon --start --background --pidfile /tmp/fluxbox.pid --make-pidfile \
  --exec /usr/bin/fluxbox
sleep 1

# Start x11vnc
start-stop-daemon --start --background --pidfile /tmp/x11vnc.pid --make-pidfile \
  --exec /usr/bin/x11vnc -- -display :1 -forever -nopw -rfbport 5900 -shared
sleep 1

# Start websockify
start-stop-daemon --start --background --pidfile /tmp/websockify.pid --make-pidfile \
  --startas /usr/bin/websockify -- --web=/usr/share/novnc 6080 localhost:5900

# Wait for port 6080 to be ready
for i in $(seq 1 30); do
  if bash -c "echo >/dev/tcp/localhost/6080" 2>/dev/null; then
    break
  fi
  sleep 1
done

echo "VNC desktop running on port 6080"
