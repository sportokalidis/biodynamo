# Demo: `standalone_visualization`

This demo showcases BioDynaMo's standalone (ParaView-free) visualization
exporter. It uses the same model as the `diffusion` demo — 8 cells at the
corners of a cube chemotaxing towards a 9th, substance-secreting cell at
the center — but its `bdm.toml` sets `adaptor = "standalone"` and enables
diffusion-field export and an extra per-agent data member, so it exercises
every export code path in `StandaloneExporter`
(`src/core/visualization/standalone/standalone_exporter.cc`): agent
VTU/PVTU files, diffusion VTI/PVTI files, and the `additional_data_members`
reflection path.

See `doc/dev_guide/standalone_visualization.md` for how the feature works.

## 0. Build BioDynaMo with the standalone exporter enabled

This demo only runs against a BioDynaMo installation built with
`-Dstandalone_visualization=ON` (which does not need ParaView or Qt):

```bash
cmake -G Ninja -Dstandalone_visualization=ON -B build
cmake --build build --parallel
. build/bin/thisbdm.sh
```

## 1. Build and run the demo

```bash
biodynamo run
```

## 2. Inspect the output

No ParaView state file is produced in this build (`bdm view` won't work
here — see `doc/dev_guide/standalone_visualization.md`). Instead, look
under `output/standalone_visualization/viz/` for the exported files:

```
agents_<step>_p<piece>.vtu       # per-thread agent piece files
agents_<step>.pvtu                # agent parallel index, one per export
diffusion_Kalium_<step>_p<piece>.vti  # per-thread diffusion piece files
diffusion_Kalium_<step>.pvti           # diffusion parallel index
```

Copy this directory to any machine with ParaView (or another
VTK-XML-compatible viewer) to open the `.pvtu`/`.pvti` files directly.
