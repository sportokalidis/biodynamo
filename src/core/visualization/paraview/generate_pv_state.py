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

RENDERLESS = os.environ.get("BDM_RENDERLESS") == "1"
os.environ.setdefault("PV_BATCH_USE_OFFSCREEN", "1")

from paraview.simple import *
# Monkey-patch BEFORE importing helpers (in case they call Show/Render later)
if RENDERLESS:
    paraview.simple._DisableFirstRenderCameraReset()

    def _no_render(*a, **k):  # no-op
        return None

    def _safe_show(src, view=None):
        v = view or GetActiveView()
        # Create/return a representation proxy without forcing a draw
        rep = GetRepresentation(src, v)
        return rep

    # prevent accidental draws anywhere
    paraview.simple.Render = _no_render
    paraview.simple.Show = _safe_show

from default_insitu_pipeline import *

def ExtractIterationFromFilename(x):
    return int(x.split('-')[-1].split('.')[0])

# ... (LoadSimulationObjectData / LoadExtracellularSubstanceData unchanged)

def BuildDefaultPipeline(json_filename):
    # ... (json and result_dir checks unchanged)

    render_view = GetActiveViewOrCreate('RenderView')
    render_view.InteractionMode = '3D'
    if RENDERLESS:
        render_view.EnableRenderOnInteraction = 0

    iterations = []

    for agent_info in build_info['agents']:
        data = LoadSimulationObjectData(result_dir, agent_info)
        ProcessSimulationObject(agent_info, data, render_view)
        iterations += [ExtractIterationFromFilename(f) for f in data.FileName]

    for substance_info in build_info['extracellular_substances']:
        data = LoadExtracellularSubstanceData(result_dir, substance_info)
        ProcessExtracellularSubstance(substance_info, data, render_view)
        iterations += [ExtractIterationFromFilename(f) for f in data.FileName]

    iterations = sorted(set(iterations))
    animation_scene = GetAnimationScene()
    tk = GetTimeKeeper()
    tk.TimestepValues = iterations
    animation_scene.NumberOfFrames = len(iterations)

    # IMPORTANT: only call this when NOT renderless
    if not RENDERLESS:
        animation_scene.UpdateAnimationUsingDataTimeSteps()

    return build_info

def WritePvsmFile(build_info):
    sim_info = build_info['simulation']
    result_dir = sim_info['result_dir']
    os.chdir(result_dir)
    SaveState(f"{sim_info['name']}.pvsm")
    # Do NOT Show(Cone()) – avoid any forced render on CI
