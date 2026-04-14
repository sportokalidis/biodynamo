# =============================================================================
# BioDynaMo Docker Image — Ready-to-Use
# =============================================================================
# Builds BioDynaMo from the local source tree so users can start working
# immediately inside the container.
#
# Build:
#   docker build \
#     --build-arg HOST_UID=$(id -u) \
#     --build-arg HOST_GID=$(id -g) \
#     -t biodynamo:latest \
#     -f docker/Dockerfile .
#
# Run:
#   docker run --name bdm -dit biodynamo:latest
#   docker exec -it bdm bash
#
# See docs/docker_biodynamo_guide.md for the full guide.
# =============================================================================

FROM ubuntu:22.04 AS base

LABEL maintainer="BioDynaMo Collaboration"
LABEL description="Ready-to-use BioDynaMo development environment"

# ---------- Avoid interactive prompts during package installation ----------
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Berlin

# ---------- Core system packages ----------
RUN apt-get update && apt-get install -y --no-install-recommends \
    apt-transport-https \
    apt-utils \
    bsdmainutils \
    ca-certificates \
    curl \
    freeglut3-dev \
    g++ \
    gcc \
    git \
    libblas-dev \
    libbz2-dev \
    libffi-dev \
    liblapack-dev \
    liblzma-dev \
    libnuma-dev \
    libomp-dev \
    libomp5 \
    libopenmpi-dev \
    libpthread-stubs0-dev \
    libreadline-dev \
    libsqlite3-dev \
    libssl-dev \
    locales \
    make \
    man \
    nano \
    ninja-build \
    python3-dev \
    python3-openssl \
    software-properties-common \
    sudo \
    tk-dev \
    wget \
    xvfb \
    xz-utils \
    zlib1g-dev \
  && rm -rf /var/lib/apt/lists/*

# ---------- X11 / OpenGL / Mesa dependencies (for ParaView) ----------
RUN apt-get update && apt-get install -y --no-install-recommends \
    libglu1-mesa \
    libx11-xcb1 \
    libxcb-glx0 \
    libxcb-icccm4 \
    libxcb-image0 \
    libxcb-keysyms1 \
    libxcb-randr0 \
    libxcb-render-util0 \
    libxcb-shape0 \
    libxcb-shm0 \
    libxcb-sync1 \
    libxcb-xfixes0 \
    libxcb-xinerama0 \
    libxcb1 \
    libxkbcommon-x11-0 \
  && rm -rf /var/lib/apt/lists/*

# ---------- Locale ----------
RUN locale-gen en_US.UTF-8
ENV LANG=en_US.UTF-8
ENV LANGUAGE=en_US:en
ENV LC_ALL=en_US.UTF-8

# ---------- Timezone (avoid tzdata prompt) ----------
RUN ln -fs /usr/share/zoneinfo/${TZ} /etc/localtime \
    && dpkg-reconfigure -f noninteractive tzdata

# ---------- CMake 3.19.3 ----------
RUN CMAKE_VER=3.19.3 \
    && curl -fsSL -O "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VER}/cmake-${CMAKE_VER}-linux-x86_64.sh" \
    && bash "cmake-${CMAKE_VER}-linux-x86_64.sh" --prefix=/usr/local --skip-license \
    && rm "cmake-${CMAKE_VER}-linux-x86_64.sh"

# ---------- Git global config (needed by BioDynaMo build) ----------
RUN git config --system user.name "BioDynaMo User" \
    && git config --system user.email "user@biodynamo.org"

# ---------- Non-root user with UID/GID mapping ----------
ARG HOST_UID=1000
ARG HOST_GID=1000

RUN groupadd -g ${HOST_GID} bdm \
    && useradd -u ${HOST_UID} -g ${HOST_GID} -m -s /bin/bash -c "BioDynaMo User" bdm \
    && passwd --delete bdm \
    && echo "bdm ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers \
    && echo "Defaults env_keep += \"BDM_LOCAL_LFS\"" >> /etc/sudoers

# ============================= BUILD STAGE ==================================
FROM base AS builder

# Copy the source tree (relies on .dockerignore to exclude build/ etc.)
COPY --chown=bdm:bdm . /opt/biodynamo

USER bdm
WORKDIR /opt/biodynamo

# ---------- Install PyEnv + Python 3.9.1 ----------
RUN curl https://pyenv.run | bash

ENV PYENV_ROOT="/home/bdm/.pyenv"
ENV PATH="${PYENV_ROOT}/bin:${PYENV_ROOT}/shims:${PATH}"

RUN eval "$(pyenv init --path)" \
    && eval "$(pyenv init -)" \
    && PYTHON_CONFIGURE_OPTS="--enable-shared" pyenv install 3.9.1 \
    && pyenv global 3.9.1 \
    && pyenv shell 3.9.1 \
    && python -m pip install --upgrade pip \
    && python -m pip install numpy

# ---------- Build BioDynaMo ----------
RUN eval "$(pyenv init --path)" \
    && eval "$(pyenv init -)" \
    && pyenv shell 3.9.1 \
    && cmake -G Ninja \
         -Dparaview=ON \
         -DCMAKE_BUILD_TYPE=Release \
         -B build \
    && cmake --build build --parallel $(( $(nproc) > 1 ? $(nproc) - 1 : 1 ))

# ============================= FINAL IMAGE ==================================
FROM base AS final

USER bdm
WORKDIR /home/bdm

# Copy built tree from builder
COPY --from=builder --chown=bdm:bdm /opt/biodynamo /opt/biodynamo
COPY --from=builder --chown=bdm:bdm /home/bdm/.pyenv /home/bdm/.pyenv

# ---------- Environment ----------
ENV PYENV_ROOT="/home/bdm/.pyenv"
ENV PATH="${PYENV_ROOT}/bin:${PYENV_ROOT}/shims:${PATH}"

# ---------- Entrypoint that sources BioDynaMo & starts Xvfb ----------
COPY --chown=bdm:bdm docker/entrypoint.sh /usr/local/bin/entrypoint.sh
RUN sudo chmod +x /usr/local/bin/entrypoint.sh

# Auto-source BioDynaMo in interactive shells
RUN echo '' >> /home/bdm/.bashrc \
    && echo '# --- BioDynaMo environment ---' >> /home/bdm/.bashrc \
    && echo 'export PYENV_ROOT="/home/bdm/.pyenv"' >> /home/bdm/.bashrc \
    && echo 'export PATH="$PYENV_ROOT/bin:$PATH"' >> /home/bdm/.bashrc \
    && echo 'eval "$(pyenv init --path)"' >> /home/bdm/.bashrc \
    && echo 'eval "$(pyenv init -)"' >> /home/bdm/.bashrc \
    && echo 'pyenv shell 3.9.1 2>/dev/null' >> /home/bdm/.bashrc \
    && echo 'if [ -f /opt/biodynamo/build/bin/thisbdm.sh ]; then' >> /home/bdm/.bashrc \
    && echo '  source /opt/biodynamo/build/bin/thisbdm.sh' >> /home/bdm/.bashrc \
    && echo 'fi' >> /home/bdm/.bashrc \
    && echo '# If DISPLAY is not set, use Xvfb virtual display' >> /home/bdm/.bashrc \
    && echo 'if [ -z "$DISPLAY" ]; then export DISPLAY=:99; fi' >> /home/bdm/.bashrc

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
CMD ["/bin/bash"]
