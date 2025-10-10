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

# This python script generates the python state from the exported files
# Therefore, the user can load the visualization simply by opening the pvsm file
# ARGUMENT: json_filename
#   json_filename: simulation information required to generate the state
#     sample files
#        {
#          "simulation": {
#              "name":"cancergrowth",
#              "result_dir":"/tmp/simulation-templates/diffusion"
#          },
#          "agents": [
#            { "name":"cell", "glyph":"Glyph", "shape":"Sphere", "scaling_attribute":"diameter_" }
#          ],
#          "extracellular_substances": [
#            { "name":"Kalium", "has_gradient":"true" }
#          ]
#        }

import glob, json, os, sys, functools

# Detect our CI-only switch
RENDERLESS = False
if len(sys.argv) >= 2 and sys.argv[1] == "--bd-renderless":
    RENDERLESS = True
    # remove the flag before normal parsing
    sys.argv.pop(1)

# Ensure pvbatch stays headless early
os.environ.setdefault("PV_BATCH_USE_OFFSCREEN", "1")

from paraview.simple import *
# If your default_insitu_pipeline import is required:
from default_insitu_pipeline import *

def ExtractIterationFromFilename(x): 
    return int(x.split('-')[-1].split('.')[0])

# --- render-less helpers ------------------------------------------------------
if RENDERLESS:
    paraview.simple._DisableFirstRenderCameraReset()

    # No-op Render
    def _no_render(*a, **k):
        return None

    # “Show” without actually rendering
    def _safe_show(src, view=None):
        v = view or GetActiveView()
        # Create (or get) a representation proxy without forcing a draw
        rep = GetRepresentation(src, v)
        return rep

    # Monkey-patch the few calls that might force a draw
    paraview.simple.Render = _no_render
    paraview.simple.Show = _safe_show

# ------------------------------------------------------------------------------

def LoadSimulationObjectData(result_dir, agent_info):
    agent_name = agent_info['name']
    files = glob.glob(f'{result_dir}/{agent_name}-*.pvtu')
    if not files:
        print(f'No data files found for agent {agent_name}')
        sys.exit(1)
    files = sorted(files, key=functools.cmp_to_key(
        lambda x, y: ExtractIterationFromFilename(x) - ExtractIterationFromFilename(y)))
    return XMLPartitionedUnstructuredGridReader(FileName=files)

def LoadExtracellularSubstanceData(result_dir, substance_info):
    substance_name = substance_info['name']
    files = glob.glob(f'{result_dir}/{substance_name}-*.pvti')
    if not files:
        print(f'No data files found for substance {substance_name}')
        sys.exit(1)
    files = sorted(files, key=functools.cmp_to_key(
        lambda x, y: ExtractIterationFromFilename(x) - ExtractIterationFromFilename(y)))
    return XMLPartitionedImageDataReader(FileName=files)

def BuildDefaultPipeline(json_filename):
    if os.path.exists(json_filename):
        with open(json_filename, 'r') as f:
            build_info = json.load(f)
    else:
        print(f'Json file {json_filename} does not exist')
        sys.exit(1)

    sim_info = build_info['simulation']
    result_dir = sim_info['result_dir']
    if result_dir and not os.path.exists(result_dir):
        print(f'Simulation result directory "{result_dir}" does not exist')
        sys.exit(1)

    # Create a view (we won’t render it in renderless mode)
    render_view = GetActiveViewOrCreate('RenderView')
    render_view.InteractionMode = '3D'
    if RENDERLESS:
        render_view.EnableRenderOnInteraction = 0

    iterations = []

    # Agents
    for agent_info in build_info['agents']:
        data = LoadSimulationObjectData(result_dir, agent_info)
        ProcessSimulationObject(agent_info, data, render_view)
        iterations += [ExtractIterationFromFilename(f) for f in data.FileName]

    # Substances
    for substance_info in build_info['extracellular_substances']:
        data = LoadExtracellularSubstanceData(result_dir, substance_info)
        ProcessExtracellularSubstance(substance_info, data, render_view)
        iterations += [ExtractIterationFromFilename(f) for f in data.FileName]

    # Set time without forcing a render
    iterations = sorted(set(iterations))
    animation_scene = GetAnimationScene()
    tk = GetTimeKeeper()
    tk.TimestepValues = iterations
    animation_scene.NumberOfFrames = len(iterations)

    # IMPORTANT: do NOT call UpdateAnimationUsingDataTimeSteps() in renderless mode
    if not RENDERLESS:
        animation_scene.UpdateAnimationUsingDataTimeSteps()

    return build_info

def WritePvsmFile(build_info):
    sim_info = build_info['simulation']
    result_dir = sim_info['result_dir']
    os.chdir(result_dir)
    SaveState(f"{sim_info['name']}.pvsm")
    # Avoid any extra Show/Render here (especially Show(Cone()))

if __name__ == '__main__':
    args = sys.argv[1:]
    if len(args) != 1:
        print("This script expects the json filename as argument.")
        sys.exit(1)
    json_filename = args[0]
    build_info = BuildDefaultPipeline(json_filename)
    WritePvsmFile(build_info)
