# BioDynaMo Docker Guide

A complete guide to running BioDynaMo inside a Docker container. By following
this guide you will get a fully built, ready-to-use BioDynaMo environment — no
manual compilation needed.

---

## Table of Contents

1. [What is Docker?](#1-what-is-docker)
2. [Prerequisites](#2-prerequisites)
3. [Installing Docker on Linux](#3-installing-docker-on-linux)
4. [Building the BioDynaMo Image](#4-building-the-biodynamo-image)
5. [Running a Container](#5-running-a-container)
6. [Entering the Container](#6-entering-the-container)
7. [Verifying BioDynaMo Works](#7-verifying-biodynamo-works)
8. [Running a Demo Simulation](#8-running-a-demo-simulation)
9. [Visualization and ParaView](#9-visualization-and-paraview)
10. [Troubleshooting](#10-troubleshooting)
11. [Cleanup](#11-cleanup)
12. [Reference: All Commands at a Glance](#12-reference-all-commands-at-a-glance)

---

## 1. What is Docker?

Docker lets you package an application together with all its dependencies into a
single, portable unit called a **container**. A container runs the same way on
any machine that has Docker installed, which means you do not need to install
BioDynaMo's dependencies manually on your system.

Key concepts:

| Term | Meaning |
|------|---------|
| **Image** | A read-only template that contains the OS, libraries, and BioDynaMo. Think of it as a snapshot. |
| **Container** | A running instance of an image. You can start, stop, and remove containers. |
| **Dockerfile** | A text file with instructions to build an image. |
| **Build context** | The set of files sent to Docker when building an image (the BioDynaMo repository). |

---

## 2. Prerequisites

| Requirement | Why |
|-------------|-----|
| **Linux** (Ubuntu 20.04+ recommended) | This guide targets Linux. macOS and Windows (WSL2) should also work but are not covered here. |
| **Docker Engine** | The container runtime, see installation below. |
| **At least 16 GB RAM** | BioDynaMo's build is memory-intensive. |
| **At least 30 GB free disk space** | The image + build artifacts need significant space. |
| **Internet connection** | Required during image build to download dependencies (CMake, PyEnv, ROOT, ParaView). |

---

## 3. Installing Docker on Linux

If Docker is already installed (`docker --version` works), skip this section.

### Ubuntu / Debian

```bash
# Update package index
sudo apt-get update

# Install prerequisites
sudo apt-get install -y ca-certificates curl gnupg

# Add Docker's official GPG key
sudo install -m 0755 -d /etc/apt/keyrings
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | \
  sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg
sudo chmod a+r /etc/apt/keyrings/docker.gpg

# Add the Docker repository
echo \
  "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] \
  https://download.docker.com/linux/ubuntu \
  $(. /etc/os-release && echo "$VERSION_CODENAME") stable" | \
  sudo tee /etc/apt/sources.list.d/docker.list > /dev/null

# Install Docker Engine
sudo apt-get update
sudo apt-get install -y docker-ce docker-ce-cli containerd.io docker-buildx-plugin
```

### Post-install: Run Docker Without `sudo`

```bash
sudo groupadd docker          # may already exist
sudo usermod -aG docker $USER
newgrp docker                 # activate group change in current shell
```

Log out and back in for the change to persist.

### Verify

```bash
docker run --rm hello-world
```

If you see "Hello from Docker!", the installation is working.

---

## 4. Building the BioDynaMo Image

Navigate to the root of the BioDynaMo repository:

```bash
cd /path/to/biodynamo
```

Build the image using the provided script:

```bash
chmod +x docker/scripts/*.sh
./docker/scripts/build_image.sh
```

Or directly with `docker build`:

```bash
docker build \
  --build-arg HOST_UID=$(id -u) \
  --build-arg HOST_GID=$(id -g) \
  -t biodynamo:latest \
  -f docker/Dockerfile .
```

**What happens during the build:**

1. Installs Ubuntu 22.04 + all system dependencies
2. Installs X11/OpenGL/Mesa libraries for ParaView
3. Installs CMake 3.19.3
4. Creates a non-root user (`bdm`) mapped to your host UID/GID
5. Installs PyEnv + Python 3.9.1
6. Copies the BioDynaMo source code into the image
7. Builds BioDynaMo with `cmake -G Ninja -Dparaview=ON -DCMAKE_BUILD_TYPE=Release`
8. Sets up automatic environment sourcing

> **Note:** The first build takes a long time (30 minutes to over an hour
> depending on your hardware and network speed). Subsequent builds use Docker's
> layer cache and are much faster.

To force a clean rebuild:

```bash
./docker/scripts/build_image.sh --no-cache
```

---

## 5. Running a Container

### Headless Mode (default, recommended)

For most users — simulations, exports, and headless visualization:

```bash
./docker/scripts/run_container.sh
```

Or directly:

```bash
docker run \
  --name bdm \
  --hostname bdm-docker \
  --cap-add=SYS_PTRACE \
  --security-opt seccomp=unconfined \
  --env BDM_HEADLESS=1 \
  -dit \
  biodynamo:latest \
  /bin/bash
```

The container starts Xvfb (a virtual framebuffer) automatically so ParaView
rendering works without a physical display.

### GUI Mode (interactive ParaView windows)

If you want to open ParaView windows on your screen:

```bash
# On the host — allow Docker to connect to your X server
xhost +local:docker

# Start container in GUI mode
./docker/scripts/run_container.sh --gui
```

Or directly:

```bash
xhost +local:docker

docker run \
  --name bdm \
  --hostname bdm-docker \
  --net=host \
  --cap-add=SYS_PTRACE \
  --security-opt seccomp=unconfined \
  --env DISPLAY=$DISPLAY \
  --volume /tmp/.X11-unix:/tmp/.X11-unix:rw \
  --device=/dev/dri:/dev/dri \
  -dit \
  biodynamo:latest \
  /bin/bash
```

> **Note on `--device=/dev/dri`:** This passes the host GPU into the container
> for hardware-accelerated OpenGL. Only include it if `/dev/dri` exists on your
> host.

---

## 6. Entering the Container

```bash
docker exec -it bdm bash
```

Or use the helper script:

```bash
./docker/scripts/exec_container.sh
```

Once inside, BioDynaMo is already sourced. You should see no errors and can
immediately use `bdm` commands.

---

## 7. Verifying BioDynaMo Works

Inside the container, run:

```bash
# Check BioDynaMo environment
echo $BDMSYS
# Expected: /opt/biodynamo/build

# Check BioDynaMo CLI
bdm config
# Expected: prints BioDynaMo configuration

# Check Python
python --version
# Expected: Python 3.9.1

# Check ROOT
root-config --version
# Expected: a version string like 6.28.12
```

Or run the automated test suite from the **host**:

```bash
./docker/scripts/test_biodynamo.sh
```

This runs 10 checks including creating and running a demo simulation.

---

## 8. Running a Demo Simulation

Inside the container:

```bash
# Create a new directory for your simulations
mkdir -p ~/simulations && cd ~/simulations

# Copy a demo project
bdm demo tumor_concept

# Enter the project directory
cd tumor_concept

# Build the simulation
bdm build

# Run the simulation
bdm run
```

Expected output: the simulation prints progress and creates output files in the
`output/` directory.

---

## 9. Visualization and ParaView

BioDynaMo uses ParaView for visualization. There are two modes:

### Headless Rendering / Export (no screen needed)

This is the default when using `BDM_HEADLESS=1`. The container starts Xvfb
automatically. BioDynaMo can export simulation data to files (VTK, PNG, etc.)
without a physical display.

This is the mode you want for:
- Running on servers
- Batch processing
- CI/CD pipelines
- Exporting images or videos programmatically

Important: `bdm view` is an interactive GUI command. In headless mode,
BioDynaMo now exits with a clear message because ParaView windows are not
visible on Xvfb.

### Interactive GUI (requires X11 forwarding)

To open ParaView windows on your host screen:

1. **On the host**, allow X11 connections:
   ```bash
   xhost +local:docker
   ```

2. Start the container in **GUI mode** (see section 5).

3. Inside the container, launch ParaView:
   ```bash
   paraview
   ```

4. For BioDynaMo demos, `bdm view` should now open the ParaView window:
  ```bash
  bdm demo tumor_concept
  cd tumor_concept
  bdm run
  bdm view
  ```

### What is installed in the image

The image includes all necessary libraries:
- `xvfb` — virtual framebuffer for headless rendering
- `freeglut3-dev` — OpenGL toolkit required by ParaView
- `libglu1-mesa` — OpenGL utilities
- `libx11-xcb1`, `libxcb-*`, `libxkbcommon-x11-0` — X11/XCB client libraries
- ParaView 5.9 + Qt5 — downloaded as part of the BioDynaMo build

### What must be done on the host (GUI mode only)

- Run `xhost +local:docker` before starting the container
- Pass `--env DISPLAY`, `--volume /tmp/.X11-unix`, and optionally `--device=/dev/dri`

---

## 10. Troubleshooting

### "Cannot connect to the Docker daemon"

```
Cannot connect to the Docker daemon at unix:///var/run/docker.sock
```

Docker is not running. Start it:
```bash
sudo systemctl start docker
```

### "Permission denied" when running `docker`

Add yourself to the `docker` group (see section 3). Then log out and log back in.

### Build fails during PyEnv / Python install

This usually means a missing system library. The Dockerfile already installs all
known prerequisites. If you see errors about `libssl`, `libbz2`, or `libffi`,
verify they are in the `apt-get install` list in the Dockerfile.

### ParaView: "cannot open display" or "no protocol specified"

**Headless mode:** Check that Xvfb is running:
```bash
pgrep Xvfb
echo $DISPLAY   # should be :99
```

If Xvfb is not running, start it manually:
```bash
Xvfb :99 -ac -screen 0 2560x1440x24 &
export DISPLAY=:99
```

**GUI mode:** Ensure you ran `xhost +local:docker` on the host and started the
container with `--gui`.

### "libGL error: No matching fbConfigs or visuals found"

This is a Mesa/OpenGL driver issue. Try:
```bash
export MESA_GL_VERSION_OVERRIDE=3.3
export LIBGL_ALWAYS_SOFTWARE=1
```

If using GUI mode, make sure `/dev/dri` is passed to the container.

### Container cannot access host files

Mount the directory when starting the container:
```bash
docker run ... --volume /path/on/host:/path/in/container ...
```

Files created inside the container have the correct permissions because the
container user UID/GID matches your host user.

### Build takes too long / runs out of memory

The build uses `nproc - 1` parallel jobs. On a machine with limited RAM, you
can reduce parallelism by editing the Dockerfile's `cmake --build` line:
```dockerfile
&& cmake --build build --parallel 2
```

---

## 11. Cleanup

### Stop and remove the container

```bash
docker rm -f bdm
```

### Remove the image

```bash
docker rmi biodynamo:latest
```

### Remove all unused Docker data

```bash
docker system prune -a
```

> **Warning:** `docker system prune -a` removes **all** unused images,
> containers, and networks. Use with caution if you have other Docker projects.

Or use the helper script:

```bash
./docker/scripts/cleanup.sh        # remove container only
./docker/scripts/cleanup.sh --all  # remove container + image
```

---

## 12. Reference: All Commands at a Glance

```bash
# Build the image
./docker/scripts/build_image.sh

# Start a container (headless)
./docker/scripts/run_container.sh

# Start a container (GUI)
xhost +local:docker
./docker/scripts/run_container.sh --gui

# Enter the container
docker exec -it bdm bash

# Run smoke tests
./docker/scripts/test_biodynamo.sh

# Stop & remove
./docker/scripts/cleanup.sh

# Full cleanup (container + image)
./docker/scripts/cleanup.sh --all
```

---

## Architecture Summary

```
┌─────────────────────────────────────────────┐
│  Host Machine                               │
│                                             │
│  ┌───────────────────────────────────────┐  │
│  │  Docker Container (bdm)               │  │
│  │                                       │  │
│  │  Ubuntu 22.04                         │  │
│  │  ├── PyEnv + Python 3.9.1             │  │
│  │  ├── CMake 3.19.3 + Ninja             │  │
│  │  ├── /opt/biodynamo/     (source)     │  │
│  │  ├── /opt/biodynamo/build/ (built)    │  │
│  │  │   ├── bin/thisbdm.sh  (sourced)    │  │
│  │  │   ├── third_party/root/            │  │
│  │  │   ├── third_party/paraview/        │  │
│  │  │   └── third_party/qt/             │  │
│  │  ├── Xvfb :99 (headless display)     │  │
│  │  └── User: bdm (UID/GID mapped)      │  │
│  └───────────────────────────────────────┘  │
│                                             │
│  Optional: X11 socket mount for GUI         │
└─────────────────────────────────────────────┘
```
