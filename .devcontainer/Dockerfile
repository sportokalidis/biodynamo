FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Berlin
ENV PYENV_ROOT="/opt/.pyenv"
ENV PATH="$PYENV_ROOT/bin:$PATH"
ENV PATH="/opt/.pyenv/versions/3.9.1/bin/:$PATH"
ENV DISPLAY=:1
ENV XDG_RUNTIME_DIR=/tmp/runtime-root

RUN unset Qt5_DIR ParaView_DIR ROOT_INCLUDE_PATH CMAKE_PREFIX_PATH PYENV_ROOT BDMSYS CC CXX

RUN apt-get -y update && \
    apt-get -y install apt-utils software-properties-common python python3 git curl make gcc g++ wget \
    wamerican libffi-dev libncurses5-dev zlib1g zlib1g-dev bzip2 aptitude libreadline-dev libssl-dev \
    libsqlite3-dev openmpi-bin libopenmpi-dev libxkbcommon-x11-dev bsdmainutils clang clang-format \
    clang-tidy doxygen graphviz libxml2-dev llvm-7 llvm-7-dev llvm-7-runtime valgrind libgsl-dev \
    freeglut3-dev libbz2-dev libnuma-dev libomp5 libomp-dev libpthread-stubs0-dev zlib1g-dev \
    libffi-dev liblzma-dev libreadline-dev libsqlite3-dev libssl-dev python-openssl tk-dev xz-utils \
    zlib1g-dev sudo libblas-dev liblapack-dev nano locales locales-all ninja-build python3-pip \
    freeglut3-dev valgrind xvfb libtbb-dev \
    x11vnc fluxbox websockify \
    novnc dbus-x11 x11-utils x11-xserver-utils xterm && \
    dpkg-reconfigure locales && locale-gen && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

RUN curl -L https://github.com/pyenv/pyenv-installer/raw/master/bin/pyenv-installer | bash && \
    eval "$(pyenv init --path)" && \
    eval "$(pyenv init -)" && \
    PYTHON_CONFIGURE_OPTS="--enable-shared" pyenv install 3.9.1 && \
    pyenv global 3.9.1 && \
    pyenv shell 3.9.1

RUN wget https://bootstrap.pypa.io/get-pip.py && \
    python3 get-pip.py && \
    python3 -m pip install -U pip && \
    python3 -m pip install numpy

RUN pip install 'cmake<4'

RUN git config --system user.name "Test User" && \
    git config --system user.email user@test.com

RUN export BDM_BRANCH="master" && \
    git clone https://github.com/BioDynaMo/biodynamo.git && \
    cd biodynamo && \
    git checkout $BDM_BRANCH && \
    git rev-parse HEAD > /commit_hash.txt && \
    cat /commit_hash.txt && \
    mkdir build && \
    cd build && \
    cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -Dtest=OFF -Dgtest=OFF -Dbenchmark=OFF \
      -Dparaview=ON -DCMAKE_POLICY_DEFAULT_CMP0048=NEW -DCMAKE_POLICY_DEFAULT_CMP0054=NEW \
      -DCMAKE_POLICY_DEFAULT_CMP0056=NEW -DCMAKE_POLICY_DEFAULT_CMP0066=NEW \
      -DCMAKE_POLICY_DEFAULT_CMP0067=NEW -DCMAKE_POLICY_DEFAULT_CMP0074=NEW .. && \
    ninja -j $(($(nproc) - 1))

RUN useradd -m -s /bin/bash codespace && \
    echo "codespace ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

RUN mkdir -p /tmp/runtime-root && chmod 700 /tmp/runtime-root

RUN echo 'source /biodynamo/build/bin/thisbdm.sh 2>/dev/null' >> /etc/bash.bashrc

COPY novnc-index.html /usr/share/novnc/index.html

RUN mkdir -p /opt/scripts
COPY start-vnc.sh /opt/scripts/start-vnc.sh
RUN chmod +x /opt/scripts/start-vnc.sh
COPY demo-picker.sh /opt/scripts/demo-picker.sh
RUN chmod +x /opt/scripts/demo-picker.sh

COPY paraview-wrapper.sh /usr/local/bin/paraview
RUN chmod +x /usr/local/bin/paraview
