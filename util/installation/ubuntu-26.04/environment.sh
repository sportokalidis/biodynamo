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

# Keep the Python 3.9 ABI used by ROOT and ParaView, with OpenSSL 3 support.
export BDM_PYTHON_VERSION=3.9.25
# ROOT's interpreter also needs the GCC 11 headers used to build the archive.
export CC="${CC:-gcc-11}"
export CXX="${CXX:-g++-11}"
# OpenMPI wrappers otherwise use Ubuntu's default compiler.
export OMPI_CC="${OMPI_CC:-$CC}"
export OMPI_CXX="${OMPI_CXX:-$CXX}"
