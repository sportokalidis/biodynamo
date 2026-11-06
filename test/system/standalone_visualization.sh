#!/bin/bash
# -----------------------------------------------------------------------------
#
# Copyright (C) 2021 CERN & University of Surrey for the benefit of the
# BioDynaMo collaboration. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
#
# See the LICENSE file distributed with this work for details.
# See the NOTICE file distributed with this work for additional information
# regarding copyright ownership.
#
# -----------------------------------------------------------------------------

set -e -x

BDM_PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../.."

. "$BDM_PROJECT_DIR/test/util.inc"

# The standalone_visualization demo only applies to a BioDynaMo installation
# built with -Dstandalone_visualization=ON; skip otherwise (e.g. the default
# -Dparaview=ON CI configuration), mirroring sbml_integration.sh's feature
# check.
set +e
bdm-config --cxxdefines | grep -q "USE_STANDALONE_VISUALIZATION"
rc_standalone=$?
set -e
if [ $rc_standalone -ne 0 ]; then
  exit 0
fi

demo_name="standalone_visualization"
demo_dir=$(mktemp -d)
biodynamo demo "${demo_name}" "${demo_dir}"
run_cmake_simulation "${demo_dir}/${demo_name}"

# Beyond run_cmake_simulation's "Simulation completed successfully!" check,
# verify the standalone exporter actually produced its output files.
if ! find "${demo_dir}/${demo_name}" -name "agents_*.pvtu" -print -quit \
    | grep -q .; then
  echo "standalone_visualization demo did not produce any agents_*.pvtu file"
  exit 1
fi
if ! find "${demo_dir}/${demo_name}" -name "diffusion_Kalium_*.pvti" \
    -print -quit | grep -q .; then
  echo "standalone_visualization demo did not produce any" \
       "diffusion_Kalium_*.pvti file"
  exit 1
fi

rm -rf "${demo_dir}"
