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

# Check if Boost was enabled for this BioDynaMo installation
set +e
bdm-config --config | grep -i boost
rc_boost=$?
set -e
if [ $rc_boost -ne 0 ]; then
  exit 0
fi

demo_name="regulatory_network"
demo_dir=$(mktemp -d)
biodynamo demo "${demo_name}" "${demo_dir}"
run_cmake_simulation "${demo_dir}/${demo_name}"
rm -rf "${demo_dir}"
