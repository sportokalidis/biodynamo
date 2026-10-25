#!/bin/bash
# Copyright (C) The BioDynaMo Project.
# Licensed under the Apache License, Version 2.0; see LICENSE and NOTICE.
# Build a relocatable C++23 ROOT package without modifying system prerequisites.
set -euo pipefail

if [[ $# -lt 1 || ! "$1" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "Usage: $0 <ROOT version, e.g. 6.36.06> [additional CMake options...]" >&2
  echo "Install prerequisites first. BDM_ROOT_WORK_DIR, BDM_ROOT_SOURCE_DIR," >&2
  echo "BDM_ROOT_PYTHON, BDM_CMAKE, CC, CXX and CMAKE_BUILD_PARALLEL_LEVEL" >&2
  echo "can select the workspace, local sources, Python, and toolchain." >&2
  exit 1
fi
ROOT_VERSION=$1
shift
BDM_PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
. "$BDM_PROJECT_DIR/util/installation/common/util.sh"
BDM_OS=$(DetectOs)
BDM_CMAKE=${BDM_CMAKE:-cmake}
BDM_ROOT_WORK_DIR=${BDM_ROOT_WORK_DIR:-"$BDM_PROJECT_DIR/build/build-third-party/root-$ROOT_VERSION-cxx23"}
BDM_ROOT_PYTHON=${BDM_ROOT_PYTHON:-python3.9}
CMAKE_BUILD_PARALLEL_LEVEL=${CMAKE_BUILD_PARALLEL_LEVEL:-2}
mkdir -p "$BDM_ROOT_WORK_DIR"
BDM_ROOT_WORK_DIR=$(cd "$BDM_ROOT_WORK_DIR" && pwd)
ROOT_INSTALL_DIR="$BDM_ROOT_WORK_DIR/install"

# These defaults can be overridden explicitly for platform-specific builds.
# Keep Python aligned with the ROOT/ParaView ABI used by BioDynaMo.
if ! PYTHON_EXECUTABLE=$(command -v "$BDM_ROOT_PYTHON"); then
  echo "Cannot find $BDM_ROOT_PYTHON. Set BDM_ROOT_PYTHON to the Python interpreter matching BioDynaMo's dependency ABI." >&2
  exit 1
fi
PYTHON_VERSION=$("$PYTHON_EXECUTABLE" -c 'import sys; print("%d.%d" % sys.version_info[:2])')
if [[ -n ${BDM_ROOT_SOURCE_DIR:-} ]]; then
  ROOTSRC=$(cd "$BDM_ROOT_SOURCE_DIR" && pwd)
else
  SOURCE_ARCHIVE="$BDM_ROOT_WORK_DIR/root_v$ROOT_VERSION.source.tar.gz"
  curl --fail --location --retry 3 \
    "https://root.cern.ch/download/root_v$ROOT_VERSION.source.tar.gz" \
    --output "$SOURCE_ARCHIVE"
  mkdir -p "$BDM_ROOT_WORK_DIR/source"
  tar -xzf "$SOURCE_ARCHIVE" -C "$BDM_ROOT_WORK_DIR/source" --strip-components=1
  ROOTSRC="$BDM_ROOT_WORK_DIR/source"
fi

"$BDM_CMAKE" -S "$ROOTSRC" -B "$BDM_ROOT_WORK_DIR/build" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$ROOT_INSTALL_DIR" \
  -DPython3_EXECUTABLE="$PYTHON_EXECUTABLE" \
  -DPYTHON_EXECUTABLE="$PYTHON_EXECUTABLE" \
  -Dpyroot=ON -Dgeom=ON -Dgdml=ON \
  "$@" -DCMAKE_CXX_STANDARD=23 -DCMAKE_CXX_STANDARD_REQUIRED=ON \
  -DCMAKE_CXX_EXTENSIONS=OFF
"$BDM_CMAKE" --build "$BDM_ROOT_WORK_DIR/build" --target install \
  --parallel "$CMAKE_BUILD_PARALLEL_LEVEL"

# Check actual output rather than trusting the archive filename or build flags.
ROOT_CONFIG="$ROOT_INSTALL_DIR/bin/root-config"
ACTUAL_ROOT_VERSION=$("$ROOT_CONFIG" --version)
if [[ "${ACTUAL_ROOT_VERSION//\//.}" != "$ROOT_VERSION" ]]; then
  echo "Requested ROOT $ROOT_VERSION but built $ACTUAL_ROOT_VERSION; refusing to mislabel the archive." >&2
  exit 1
fi
ROOT_FLAGS=$("$ROOT_CONFIG" --cflags)
if [[ ! "$ROOT_FLAGS" =~ -std=(c|gnu)\+\+(23|2b)($|[[:space:]]) ]]; then
  echo "ROOT did not build in C++23 mode: $ROOT_FLAGS" >&2
  exit 1
fi
# Run in a subprocess so ROOT's environment changes do not leak into packaging.
(
  set +u
  . "$ROOT_INSTALL_DIR/bin/thisroot.sh"
  "$ROOT_INSTALL_DIR/bin/root" -l -b -q \
    -e 'if (__cplusplus <= 202002L) gSystem->Exit(1);'
)

# ROOT bakes its build compiler's __cplusplus into the public RConfigure.h and
# warns from that header whenever a consumer disagrees. Cling reports the final
# C++23 value, so a package built by a compiler still reporting the provisional
# 202100L only suits that same compiler and warns during every interpretation.
ROOT_CPLUSPLUS=$(sed -n \
  's/^[[:space:]]*#[[:space:]]*define[[:space:]]\+ROOT__cplusplus[[:space:]]\+\([0-9]\+\)L\?[[:space:]]*$/\1/p' \
  "$ROOT_INSTALL_DIR/include/RConfigure.h")
if [[ "$ROOT_CPLUSPLUS" != "202302" ]]; then
  echo "ROOT reports __cplusplus=${ROOT_CPLUSPLUS:-unknown} in RConfigure.h, not the" >&2
  echo "final C++23 value 202302. Rebuild with a compiler reporting 202302 (GCC 14" >&2
  echo "or newer, Clang 17 or newer) so compiled consumers and Cling agree; refusing" >&2
  echo "to package a compiler-specific archive." >&2
  exit 1
fi

ARCH=$(uname -m)
RESULT_FILE="root_v${ROOT_VERSION}_cxx23_python${PYTHON_VERSION}_${BDM_OS}-${ARCH}.tar.gz"
MANIFEST="$ROOT_INSTALL_DIR/bdm-build-manifest.txt"
{
  echo "Requested ROOT release: $ROOT_VERSION"
  echo "OS/architecture: $BDM_OS/$ARCH"
  "$ROOT_CONFIG" --version
  "$ROOT_CONFIG" --cflags
  "$ROOT_CONFIG" --features
  echo "ROOT__cplusplus: $ROOT_CPLUSPLUS"
  "$BDM_CMAKE" --version
  "${CXX:-c++}" --version
  "$PYTHON_EXECUTABLE" --version
  echo "Configure cache:"
  cat "$BDM_ROOT_WORK_DIR/build/CMakeCache.txt"
} > "$MANIFEST"
tar -czf "$BDM_ROOT_WORK_DIR/$RESULT_FILE" -C "$ROOT_INSTALL_DIR" .
(cd "$BDM_ROOT_WORK_DIR" && shasum -a 256 "$RESULT_FILE" > "$RESULT_FILE.sha256")
echo "Archive: $BDM_ROOT_WORK_DIR/$RESULT_FILE"
echo "Checksum: $BDM_ROOT_WORK_DIR/$RESULT_FILE.sha256"
echo "Use this archive with BDM_ROOT_URL and BDM_ROOT_SHA256 before publishing it."
