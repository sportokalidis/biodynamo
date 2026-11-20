# Checking out the standalone exporter

This branch adds a new way to export simulation results to files for
viewing later, without needing to install ParaView. This guide gives you
the exact commands to build it and run a demo. You do not need any
BioDynaMo or C++ knowledge to follow it.

## 1. Get the code

```bash
git clone https://github.com/sportokalidis/biodynamo.git
cd biodynamo
git checkout paraview-exporter-opt
```

(If you already have a clone, just run `git fetch origin paraview-exporter-opt`
and `git checkout paraview-exporter-opt` inside it instead.)

## 2. Install prerequisites (first time only)

```bash
./prerequisites.sh all
```

This installs the system packages BioDynaMo needs. It can take a while
the first time.

## 3. Build BioDynaMo with the standalone exporter

```bash
cmake -G Ninja -Dstandalone_visualization=ON -DCMAKE_BUILD_TYPE=Release -B build
cmake --build build --parallel
```

This build does **not** download or install ParaView, so it is faster and
lighter than the default build.

## 4. Load the environment

Run this in every new terminal before using BioDynaMo:

```bash
source build/bin/thisbdm.sh
```

## 5. Run the demo

```bash
biodynamo demo standalone_visualization
cd standalone_visualization
biodynamo run
```

Exported files appear under `output/standalone_visualization/viz/`. If
you have ParaView installed on any machine, you can open the `.pvtu` /
`.pvti` files directly to view the simulation, even though the build
that produced them never installed ParaView itself.
