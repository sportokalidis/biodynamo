# Standalone Visualization Adaptor — Developer Reference

**Branch:** `paraview-exporter-opt`  
**Author:** sportokalidis  
**Scope:** Everything introduced by the three commits on this branch that implement `src/core/visualization/standalone/`.

---

## Table of Contents

1. [Why This Exists — The Problem Statement](#1-why-this-exists)
2. [How BioDynaMo Visualization Works — The Full Pipeline](#2-how-biodynamo-visualization-works)
3. [CMake Changes](#3-cmake-changes)
4. [The Plugin System — How an Adaptor Gets Loaded at Runtime](#4-the-plugin-system)
5. [File-by-File Walkthrough: `visualization/standalone/`](#5-file-by-file-walkthrough-standalone)
   - [adaptor.h](#51-adaptorh)
   - [adaptor.cc](#52-adaptorcc)
   - [standalone_exporter.h](#53-standalone_exporterh)
   - [standalone_exporter.cc — WriteStep (agents)](#54-standalone_exportercc--writestep)
   - [standalone_exporter.cc — WriteDiffusionStep (diffusion grids)](#55-standalone_exportercc--writediffusionstep)
6. [Comparison: Standalone vs ParaView Adaptor](#6-comparison-standalone-vs-paraview-adaptor)
7. [VTK File Formats Explained](#7-vtk-file-formats-explained)
8. [Output File Structure](#8-output-file-structure)
9. [How to Build and Enable](#9-how-to-build-and-enable)
10. [Configuration Reference (`bdm.toml`)](#10-configuration-reference)
11. [Changes to Existing Files](#11-changes-to-existing-files)

---

## 1. Why This Exists

BioDynaMo's standard visualization path requires a pre-built copy of **ParaView** (~500 MB) installed as a third-party dependency. ParaView provides the VTK library, which BioDynaMo uses to construct in-memory data structures (`vtkImageData`, `vtkUnstructuredGrid`, etc.) and write them to disk via ParaView's Catalyst pipeline.

This works well but has two downsides:

- **Deployment cost.** Every machine that needs to export visualization files must have the ParaView installation, including CI/CD pipelines that only care about simulation output.
- **Coupling.** The visualization export path is tightly coupled to VTK's C++ object model, which requires Qt/OpenGL headers even in pure export (no-display) mode.

The **standalone visualization adaptor** is a replacement that:

1. Writes the same VTK XML file formats (`.vtu`, `.pvtu`) that ParaView produces, but **using only the C++ standard library** — no VTK, no Qt, no ParaView headers.
2. Is compiled into its own shared library (`libVisualizationAdaptor.so`) which BioDynaMo loads at runtime through the same plugin mechanism used for the ParaView adaptor, so the rest of the codebase is **completely unchanged**.
3. Supports **multi-threaded, parallelised export** using OpenMP, writing each piece of the domain to its own file in parallel.

---

## 2. How BioDynaMo Visualization Works

Before reading any code it is essential to understand the overall pipeline, because many design decisions only make sense in that context.

```
Simulation::GetScheduler()->Simulate(N)
          │
          │  every `visualization_interval` steps
          ▼
  VisualizationOp::Execute()          [src/core/operation/visualization_op.h]
          │
          │  calls
          ▼
  VisualizationAdaptor::Visualize()   [pure virtual — implemented by plugin]
          │
          ├── ParaviewAdaptor::Visualize()   [paraview build]
          │         uses vtkImageData, vtkUnstructuredGrid, Catalyst…
          │
          └── StandaloneAdaptor::Visualize() [standalone build — THIS BRANCH]
                    uses StandaloneExporter (plain C++/STL)
```

The key abstraction is `VisualizationAdaptor` — a pure abstract class with one method: `virtual void Visualize()`. The concrete implementation is a **ROOT plugin** — a shared library that is loaded at runtime via ROOT's `TPluginManager`. This is why you can switch between ParaView and standalone just by changing a single line in `bdm.toml` without recompiling your simulation.

### Key Parameters (from `Param`)

| TOML key | C++ field | Meaning |
|---|---|---|
| `visualization.export` | `export_visualization` | Whether to write files to disk at all |
| `visualization.interval` | `visualization_interval` | How many time steps between exports |
| `visualization.adaptor` | `visualization_engine` | Which plugin to load: `"paraview"` or `"standalone"` |
| `[[visualize_agent]]` | `visualize_agents` | Which agent types (and extra fields) to export |
| `[[visualize_diffusion]]` | `visualize_diffusion` | Which diffusion substances to export |

---

## 3. CMake Changes

All CMake changes are in the top-level `CMakeLists.txt`.

### 3.1 New Option

```cmake
# CMakeLists.txt line 142
option(standalone_visualization
       "Enable standalone VTU/PVTU exporter (no ParaView/Qt)" OFF)
```

This declares a new **CMake boolean option** named `standalone_visualization`, with a default of `OFF`. You enable it by passing `-Dstandalone_visualization=ON` to `cmake`. When it is `OFF` the entire standalone code is ignored and the regular ParaView build path is used.

### 3.2 Skip GLUT Check When Using Standalone

```cmake
# Before (original):
if(paraview)
  find_package(GLUT) ...

# After (this branch):
if(paraview AND NOT standalone_visualization)
  find_package(GLUT) ...
```

GLUT (OpenGL Utility Toolkit) is a display library required by ParaView's rendering pipeline. If `standalone_visualization=ON` we do not need a display at all — we are only writing files — so the GLUT check is skipped. Without this guard, `cmake` would either fail or disable ParaView even when you explicitly passed `-Dstandalone_visualization=ON` because GLUT is not installed on headless servers.

### 3.3 Exclude Standalone Sources from the Core Library

```cmake
# CMakeLists.txt ~line 591
filter_list(LIB_SOURCES "${LIB_SOURCES}" "standalone/*")
filter_list(HEADERS     "${HEADERS}"     "standalone/*")
```

`filter_list` is a BioDynaMo CMake helper that removes entries matching a glob pattern from a list. Without these two lines, the files in `src/core/visualization/standalone/` would be compiled **twice**: once into `libbiodynamo.so` (the core library) and once into `libVisualizationAdaptor.so` (the plugin). Double-compilation would cause duplicate symbol linker errors. These lines ensure the standalone files only go into the plugin.

The parallel lines for ParaView sources already existed above:

```cmake
filter_list(LIB_SOURCES "${LIB_SOURCES}" "paraview/*")
filter_list(HEADERS     "${HEADERS}"     "paraview/*")
```

### 3.4 Build the Standalone Plugin Library

```cmake
# CMakeLists.txt ~line 643
if(standalone_visualization)
  message(STATUS "Building standalone visualization adaptor (ParaView disabled)")

  # Collect all .h and .cc files under src/core/visualization/standalone/
  file(GLOB_RECURSE SA_HEADERS
       "${CMAKE_SOURCE_DIR}/src/core/visualization/standalone/*.h")
  file(GLOB_RECURSE SA_SOURCES
       "${CMAKE_SOURCE_DIR}/src/core/visualization/standalone/*.cc")

  # Build a ROOT-loadable shared library called "VisualizationAdaptor"
  build_shared_library(VisualizationAdaptor
                    SELECTION selection-libVisualizationAdaptor.xml
                    SOURCES   ${SA_SOURCES}
                    HEADERS   ${SA_HEADERS}
                    LIBRARIES biodynamo
                    PLUGIN    "TRUE")

  # Linux linker flag: --no-as-needed forces all symbols to be loaded even if
  # not directly referenced, which ROOT's plugin system requires.
  if(LINUX)
    SET_TARGET_PROPERTIES(VisualizationAdaptor PROPERTIES
      LINK_FLAGS "-Wl,--no-as-needed")
  else()
    SET_TARGET_PROPERTIES(VisualizationAdaptor PROPERTIES
      LINK_FLAGS "-dynamic")  # macOS equivalent
  endif()
endif()
```

`build_shared_library` is a BioDynaMo CMake function that:
- Compiles the listed source files into a shared library (`libVisualizationAdaptor.so`).
- Runs ROOT's dictionary generator (`rootcling`) on the headers listed in `SELECTION` XML to produce reflection metadata. ROOT needs this to be able to call `bdm::StandaloneAdaptor::Factory()` at runtime without a compile-time dependency.
- Installs the `.so` into `build/lib/`.

The `PLUGIN "TRUE"` flag tells the function to also install a ROOT plugin registration file under `build/etc/plugins/`.

### 3.5 Mark `with_paraview` as Off

```cmake
# Before:
if(NOT paraview)
  set(with_paraview OFF)
endif()

# After:
if(NOT paraview OR standalone_visualization)
  set(with_paraview OFF)
endif()
```

The `with_paraview` CMake variable is written into `thisbdm.sh` and used by the `biodynamo` CLI to decide whether ParaView-specific Python scripts are invoked. When building with `standalone_visualization=ON`, we always set it to `OFF` regardless of the `paraview` flag, so the CLI does not try to call ParaView on the output files.

---

## 4. The Plugin System

Understanding how ROOT's `TPluginManager` works is essential, because it is the glue between `VisualizationAdaptor::Create()` and `StandaloneAdaptor::Factory()`.

### 4.1 What a ROOT Plugin Is

A ROOT plugin is a shared library that implements a class which inherits from an abstract base class. ROOT can load this shared library at runtime, instantiate the class by calling a named factory function, and return the result as a pointer to the base class — without the calling code ever needing to `#include` the implementation header.

In BioDynaMo's case:
- **Base class:** `VisualizationAdaptor` (abstract, one virtual method `Visualize()`)
- **Plugin name:** `"VisualizationAdaptor"` (used as the plugin category)
- **Adaptor names:** `"paraview"` or `"standalone"` (used as the regex to select which plugin to load)
- **Factory function:** `Factory()` (static method on each concrete class)

### 4.2 Plugin Registration File

ROOT discovers plugins by scanning directories listed in `PluginPath` (read from `bdm.rootrc`). Each plugin is described by a small C macro file that ROOT executes with its embedded interpreter (cling):

```
build/etc/plugins/VisualizationAdaptor/P000_StandaloneAdaptor.C
```

```cpp
void P000_StandaloneAdaptor() {
  // Handler is registered programmatically in VisualizationAdaptor::Create().
}
```

This file is intentionally **empty** (just a valid function body). The actual `AddHandler()` call that registers the standalone plugin is done in C++ code (see Section 4.3), because ROOT's cling interpreter sometimes fails to parse more complex macro files on certain Linux distributions. Having an empty-but-parseable macro file prevents ROOT from printing `"cannot open source file"` errors while still deferring actual registration to reliable C++ code.

### 4.3 `VisualizationAdaptor::Create()` — The Factory

`src/core/visualization/visualization_adaptor.cc` is the entry point for loading any adaptor. Here is an annotated walkthrough of the function added/modified on this branch:

```cpp
VisualizationAdaptor* VisualizationAdaptor::Create(const std::string& adaptor) {
  auto* param = Simulation::GetActive()->GetParam();

  // If neither insitu nor export is enabled, visualization is turned off.
  // Return nullptr immediately so the scheduler skips calling Visualize().
  if (!(param->insitu_visualization || param->export_visualization))
    return nullptr;

  // NEW (this branch): Manually load bdm.rootrc so ROOT knows where to look
  // for plugin files. This is needed because on some systems the file is not
  // automatically read before TPluginManager scans for handlers.
  const char* bdmsys = std::getenv("BDMSYS");
  if (bdmsys) {
    std::string rc = std::string(bdmsys) + "/etc/bdm.rootrc";
    gEnv->ReadFile(rc.c_str(), kEnvUser);
  }

  bool first_try = !loaded_.count(adaptor);
  if (first_try) {
    // Ask ROOT's plugin manager for a handler matching the adaptor name.
    // ROOT scans the PluginPath directories for .C files whose AddHandler()
    // calls registered a handler with a regex matching `adaptor`.
    auto* h = gPluginMgr->FindHandler("VisualizationAdaptor", adaptor.c_str());

    // NEW (this branch): If ROOT did not find the handler via the .C file
    // (because the file is empty), register it programmatically here.
    // This calls gPluginMgr->AddHandler(category, regex, class, library, ctor)
    //   category = "VisualizationAdaptor"  — the plugin type
    //   regex    = "standalone"            — matched against the adaptor name
    //   class    = "bdm::StandaloneAdaptor"— the C++ class to instantiate
    //   library  = "VisualizationAdaptor"  — the .so filename (without lib prefix)
    //   ctor     = "Factory()"             — the factory function to call
    if (!h && adaptor == "standalone") {
      gPluginMgr->AddHandler("VisualizationAdaptor", "standalone",
                             "bdm::StandaloneAdaptor", "VisualizationAdaptor",
                             "Factory()");
      h = gPluginMgr->FindHandler("VisualizationAdaptor", adaptor.c_str());
    }

    if (h) {
      // LoadPlugin() dlopen()s libVisualizationAdaptor.so and resolves symbols.
      if (h->LoadPlugin() == 0) {
        loaded_[adaptor] = h;             // cache the handler for future steps
        va = reinterpret_cast<VisualizationAdaptor*>(h->ExecPlugin(0));
        // ExecPlugin(0) calls Factory() via cling, returns a new instance.
        return va;
      }
      // ...
    }
  } else {
    // On subsequent calls (steps 2, 3, …) the library is already loaded.
    // Just call Factory() again to get a new instance.
    if (loaded_[adaptor])
      return reinterpret_cast<VisualizationAdaptor*>(
          loaded_[adaptor]->ExecPlugin(0));
  }
}
```

---

## 5. File-by-File Walkthrough: Standalone

### 5.1 `adaptor.h`

```cpp
#ifndef BDM_SRC_CORE_VISUALIZATION_STANDALONE_ADAPTOR_H_
#define BDM_SRC_CORE_VISUALIZATION_STANDALONE_ADAPTOR_H_

#include "core/util/root.h"               // BDM_CLASS_DEF_NV macro
#include "core/visualization/visualization_adaptor.h"  // abstract base class

namespace bdm {

class StandaloneExporter;  // forward declaration — avoids pulling in the
                            // exporter header everywhere adaptor.h is included

class StandaloneAdaptor : public VisualizationAdaptor {
 public:
  static StandaloneAdaptor* Factory();  // required by ROOT plugin system —
                                         // ROOT calls this to instantiate the class
  StandaloneAdaptor();
  ~StandaloneAdaptor() override;

  void Visualize() override;  // called every exported time step by the scheduler

 private:
  bool initialized_ = false;        // true after the output directory is created
  StandaloneExporter* exporter_ = nullptr;  // owns the writer object

  BDM_CLASS_DEF_NV(StandaloneAdaptor, 1);
  // This ROOT macro generates the RTTI/reflection dictionary entry that allows
  // ROOT's cling interpreter to call Factory() by name at runtime.
  // "NV" means "No Virtual table" for ROOT serialization purposes.
  // The "1" is the class version number for ROOT I/O.
};

}  // namespace bdm
#endif
```

### 5.2 `adaptor.cc`

```cpp
#include "core/visualization/standalone/adaptor.h"
#include "core/simulation.h"    // Simulation::GetActive()
#include "core/scheduler.h"     // GetSimulatedSteps()
#include "core/param/param.h"   // export_visualization, visualization_interval
#include <filesystem>           // std::filesystem::create_directories
#include "core/visualization/standalone/standalone_exporter.h"

namespace bdm {

// Factory() is what ROOT calls when loading the plugin.
// It simply allocates a new StandaloneAdaptor on the heap.
// The caller (VisualizationAdaptor::Create) owns the returned pointer.
StandaloneAdaptor* StandaloneAdaptor::Factory() {
  return new StandaloneAdaptor();
}

StandaloneAdaptor::StandaloneAdaptor() = default;

// The destructor frees the exporter, which closes any open file handles.
StandaloneAdaptor::~StandaloneAdaptor() {
  delete exporter_;
}

void StandaloneAdaptor::Visualize() {
  auto* sim   = Simulation::GetActive();
  auto* param = sim->GetParam();

  // Lazy initialisation: the output directory and the exporter object are
  // created on the first call to Visualize(), not in the constructor.
  // This ensures sim->GetOutputDir() is valid (the simulation is already
  // running and the output directory has been created by the scheduler).
  if (!initialized_) {
    std::string out_dir = sim->GetOutputDir() + "/viz";
    // create_directories is equivalent to "mkdir -p" — creates the full path
    // even if intermediate directories do not exist yet.
    std::filesystem::create_directories(out_dir);
    exporter_ = new StandaloneExporter(out_dir);
    initialized_ = true;
  }

  // Check the two gating conditions before doing any I/O:
  //   1. export_visualization must be true  (set in bdm.toml as export = true)
  //   2. The current step must be a multiple of visualization_interval
  //      (set in bdm.toml as interval = N)
  // This mirrors the logic in the ParaView adaptor so that both adaptors
  // produce files at exactly the same time steps.
  uint64_t total_steps = sim->GetScheduler()->GetSimulatedSteps();
  if (param->export_visualization &&
      (total_steps % param->visualization_interval == 0)) {
    exporter_->WriteStep();          // write agent positions and attributes
    exporter_->WriteDiffusionStep(); // write diffusion grid concentrations
  }
}

}  // namespace bdm
```

### 5.3 `standalone_exporter.h`

```cpp
class StandaloneExporter {
 public:
  // output_dir: the directory where all output files will be written.
  explicit StandaloneExporter(const std::string& output_dir);
  ~StandaloneExporter();

  // Writes one time step of agent data.
  // Produces:  agents_{step}_p{piece}.vtu  (one per thread)
  //            agents_{step}.pvtu          (parallel index file)
  void WriteStep();

  // Dispatcher: writes diffusion data and increments step_.
  // Calls WriteDiffusionStepVti() by default (VTK ImageData output).
  // Replace with WriteDiffusionStepVtu() to switch to VTK_VOXEL output.
  void WriteDiffusionStep();

 private:
  std::string output_dir_;
  int step_ = 0;  // incremented by WriteDiffusionStep() after both file families are written

  // Scalar member names read from additional_data_members in bdm.toml.
  // Loaded once in the constructor; bdm.toml does not change at runtime.
  std::vector<std::string> extra_members_;

  // Agent parallel index file.
  void WritePvtu(int pieces) const;

  // VTK ImageData (VTI) diffusion output — default.
  void WriteDiffusionStepVti();

  // VTK UnstructuredGrid with VTK_VOXEL cells — available alternative.
  void WriteDiffusionStepVtu();

  // VTI parallel index file for one diffusion substance.
  void WriteDiffusionPvti(const std::string& name,
                          bool has_concentration, bool has_gradient,
                          std::size_t nx, std::size_t ny, std::size_t nz,
                          double ox, double oy, double oz,
                          double spacing, int pieces,
                          std::size_t boxes_per_piece) const;

  // VTU parallel index file for one diffusion substance.
  void WriteDiffusionVtuIndex(const std::string& name,
                              bool has_concentration, bool has_gradient,
                              int pieces) const;
};
```

### 5.4 `standalone_exporter.cc` — WriteStep

`WriteStep` exports the positions and attributes of every agent (cell) in the simulation.

#### Phase 1: Collect all agent data into flat arrays

```cpp
void StandaloneExporter::WriteStep() {
  auto* sim = Simulation::GetActive();
  auto* rm  = sim->GetResourceManager();

  // Flat arrays for the entire agent population.
  // Using flat arrays (not a vector of structs) allows us to later slice
  // contiguous sub-ranges per piece with a single pointer offset.
  std::vector<Agent*>   agents;
  std::vector<double>   points;   // x0,y0,z0, x1,y1,z1, …  (3 doubles per agent)
  std::vector<uint64_t> ids;

  // ForEachAgent iterates over ALL agents across all NUMA domains.
  rm->ForEachAgent([&](Agent* a) {
    agents.push_back(a);
    const auto& pos = a->GetPosition();
    points.push_back(pos[0]);
    points.push_back(pos[1]);
    points.push_back(pos[2]);
    ids.push_back(a->GetUid().GetIndex());  // unique integer ID of this agent
  });

  const size_t n = ids.size();  // total number of agents

  // Pre-extract Diameter into a flat array before the parallel loop.
  // All agent reads happen on the main thread; each parallel thread
  // then only reads from its own read-only slice — no mutexes needed.
  std::vector<double> diam(n);
  for (size_t i = 0; i < n; ++i)
    diam[i] = agents[i]->GetDiameter();
```

#### Phase 2: Read extra fields from `extra_members_`

```cpp
  // extra_members_ was populated once in the constructor from bdm.toml.
  // No file I/O happens here — we just use the cached field names.
  // ReadScalarMember uses ROOT's TDataMember::GetOffset() to find each
  // field's byte offset in the actual derived type at runtime.
  std::vector<std::vector<double>> extra_vals(extra_members_.size());
  for (size_t f = 0; f < extra_members_.size(); ++f) {
    extra_vals[f].resize(n);
    for (size_t i = 0; i < n; ++i) {
      double val = 0.0;
      ReadScalarMember(agents[i], extra_members_[f], val);
      extra_vals[f][i] = val;
    }
  }
```

#### Phase 3: Partition agents into pieces and write in parallel

```cpp
  // Each OMP thread writes one piece (one .vtu file).
  // num_pieces is at most the number of CPU threads, but also capped at n
  // (no empty pieces if there are fewer agents than threads).
  auto* tinfo = ThreadInfo::GetInstance();
  uint64_t max_threads  = tinfo->GetMaxThreads();
  uint64_t num_pieces   = std::max<uint64_t>(1, std::min<uint64_t>(max_threads, n));
  uint64_t per_piece    = (n + num_pieces - 1) / num_pieces; // ceiling division

  #pragma omp parallel for schedule(static, 1)
  for (uint64_t p = 0; p < num_pieces; ++p) {
    uint64_t begin = p * per_piece;
    if (begin >= n) continue;           // last few threads may have empty ranges
    uint64_t count = std::min<uint64_t>(per_piece, n - begin);
```

#### Phase 4: Build and write one VTU file

```cpp
    // agents_{step}_p{piece}.vtu
    std::ofstream vtu(vtu_name.str(), std::ios::binary);

    // VTU files contain an XML header describing the data layout, followed
    // by a raw binary block (AppendedData) holding the actual numbers.
    // The XML section uses byte offsets to reference where each array starts
    // in the binary block. All offsets are computed before writing any XML.

    // `append_array` is a local lambda that:
    //   1. Computes the byte size of the array  (elems * components * elem_size)
    //   2. Generates the XML <DataArray ...> tag with the current offset
    //   3. Stores (tag, byte_size, data_pointer) in `metas` for later
    //   4. Advances `offset` by (4 + byte_size) — the 4 bytes are the
    //      per-array length prefix that VTK's raw appended format requires
    struct ArrayMeta { std::string tag; uint32_t nbytes; const char* data; };
    std::vector<ArrayMeta> metas;
    uint32_t offset = 0;

    auto append_array = [&](const std::string& type, const std::string& name,
                            int components, const void* buf,
                            size_t elems, size_t elem_size) {
      uint32_t bytes = elems * components * elem_size;
      // Build the XML tag referencing the current binary offset.
      // The offset is relative to the start of the binary block (after "_").
      tag << "<DataArray type=\"" << type << "\" Name=\"" << name
          << "\" NumberOfComponents=\"" << components
          << "\" format=\"appended\" offset=\"" << offset << "\"/>";
      metas.push_back({tag.str(), bytes, reinterpret_cast<const char*>(buf)});
      offset += 4 + bytes;  // 4-byte length header + payload
    };
```

```cpp
    // Agent data is stored as PointData — one value per agent (point).
    // VTK_VERTEX cells (type 1) are used: each cell has exactly one node,
    // so "number of points" == "number of cells".
    // This allows attaching data to agents as PointData, which ParaView
    // can directly colour-map and filter.
    vtu << "<Piece NumberOfPoints=\"" << count
        << "\" NumberOfCells=\""  << count << "\">\n";

    // PointData section: all per-agent attributes.
    // Cell_ID and Diameter are always written; extra_members_ supplies any
    // additional scalar fields the user listed in bdm.toml.
    append_array("UInt64",  "Cell_ID",  1, &ids[begin],  count, 8);
    append_array("Float64", "Diameter", 1, &diam[begin], count, 8);
    for (size_t f = 0; f < extra_members_.size(); ++f)
      append_array("Float64", extra_members_[f], 1,
                   &extra_vals[f][begin], count, 8);

    // Points section: XYZ coordinates
    // (sliced from the flat `points` array using the piece's begin index)

    // Cells section: topology arrays for VTK_VERTEX
    //   connectivity[i] = i          (each cell references exactly its own point)
    //   offsets[i]      = i + 1      (cumulative count of nodes per cell)
    //   types[i]        = 1          (VTK_VERTEX = type code 1)
    for (uint64_t i = 0; i < count; ++i) {
      conn[i]      = i;
      offs[i]      = i + 1;
      types_arr[i] = 1;
    }
```

#### Phase 5: Write the binary AppendedData block

```cpp
    // The AppendedData section starts with "_" (required by the VTK spec).
    // Each array is written as: [4-byte uint32 length][raw bytes]
    vtu << "  <AppendedData encoding=\"raw\">\n_";
    for (auto& m : metas) {
      uint32_t sz = m.nbytes;
      vtu.write(reinterpret_cast<const char*>(&sz), 4);  // length prefix
      vtu.write(m.data, sz);                               // actual data
    }
```

After all pieces are written, the parallel loop ends and a single PVTU index file is written on the main thread.

#### `LoadAdditionalMembers()` helper (called once from constructor)

```cpp
static std::vector<std::string> LoadAdditionalMembers() {
  // Opens bdm.toml in the current working directory (where the simulation
  // binary was launched from) and extracts the names listed under
  // `additional_data_members = ["field1_", "field2_"]`.
  // Uses a single regex pass — no TOML library dependency.
  // The result is stored in extra_members_ and reused every step.
  std::regex re(R"(additional_data_members\s*=\s*\[([^\]]+)\])");
  // ...returns vector of field name strings
}
```

#### `ReadScalarMember()` helper

```cpp
static bool ReadScalarMember(const Agent* agent,
                              const std::string& name, double& out) {
  // TClass::GetClass(typeid(*agent)) returns ROOT's reflection descriptor
  // for the actual runtime type (e.g. "MyCell"), not the base class.
  TClass*      cls = TClass::GetClass(typeid(*agent));
  TDataMember*  dm = cls->GetDataMember(name.c_str());

  // dm->GetOffset() is the byte offset of the field from the start of the
  // object — equivalent to offsetof() but resolved at runtime.
  const char* addr = reinterpret_cast<const char*>(agent) + dm->GetOffset();

  // Read the raw bytes at that offset as the correct numeric type and
  // convert to double for storage in the VTU file.
  if (tname == "double")  { out = *reinterpret_cast<const double*>(addr); }
  if (tname == "float")   { out = *reinterpret_cast<const float*>(addr);  }
  // ...
}
```

### 5.5 `standalone_exporter.cc` — WriteDiffusionStep

`WriteDiffusionStep` is a thin dispatcher that calls `WriteDiffusionStepVti()` and then increments `step_`.  Two complete implementations exist: the VTI implementation is the default; the VTU implementation is kept as an alternative and can be activated by swapping the call inside `WriteDiffusionStep()`.

#### Background: What a Diffusion Grid Is

BioDynaMo's `DiffusionGrid` is a uniform 3D array of boxes (voxels). Each box stores:
- `concentration`: the scalar value of the substance in that box.
- `gradient`: a 3-component vector (∂c/∂x, ∂c/∂y, ∂c/∂z) at the box center.

The grid has `nx × ny × nz` boxes and is described by:
- `dims[0], dims[2], dims[4]`: the minimum x, y, z coordinate of the grid boundary.
- `box`: the side length of each cubic box.

#### Parallelisation Strategy: Z-slab Decomposition

The grid is sliced into horizontal slabs along the Z axis — one slab per thread.  Each thread independently writes its own piece file.

```
Z axis
  │
  │  ┌─────────────────┐  ← slab p=num_pieces-1  (last)
  │  │    k slabs...   │
  │  ├─────────────────┤
  │  │    k slabs...   │
  │  ├─────────────────┤
  │  │    k slabs...   │
  └─►└─────────────────┘  ← slab p=0  (first, k_begin=0)
```

#### `WriteDiffusionStepVti()` — default VTK ImageData output

Produces `.vti` + `.pvti` files.  The grid is encoded with `Origin` at the first box centre, `Spacing` = box length, and `WholeExtent="0 nx-1 0 ny-1 0 nz-1"` (N points, not N+1 nodes).  Data is stored as `PointData`: one VTK "node" per BioDynaMo box centre — no explicit geometry arrays are written.

Adjacent pieces share one boundary point (`vtkXMLPImageDataReader` requires this to assemble the dataset without gaps).  Non-last pieces write `k_len + 1` layers of box-centre values, where the extra layer is a zero-copy read from the grid's own storage:

```cpp
// Origin = centre of the first diffusion box so that VTK "nodes" sit
// exactly at BioDynaMo's box centres.
const double ox = dims[0] + 0.5 * box;
const double oy = dims[2] + 0.5 * box;
const double oz = dims[4] + 0.5 * box;

// Non-last pieces include one shared-boundary node; the last piece ends at nz-1.
uint64_t k_end;
uint64_t layers;
if (k_begin + k_len < nz) {
  k_end  = k_begin + k_len;  // shared boundary with next piece
  layers = k_len + 1;
} else {
  k_end  = nz - 1;           // last piece — no extra node
  layers = k_len;
}

// WholeExtent uses N points per dimension (N box centres, not N+1 nodes).
vti << "  <ImageData"
    << " WholeExtent=\"0 " << nx-1 << " 0 " << ny-1 << " 0 " << nz-1 << "\""
    << " Origin=\""  << ox << " " << oy << " " << oz << "\""
    << " Spacing=\"" << box << " " << box << " " << box << "\">\n";
vti << "    <Piece Extent=\"0 " << nx-1 << " 0 " << ny-1
    << " " << k_begin << " " << k_end << "\">\n";
vti << "      <PointData>\n";

// Grid layout is Z-outer, Y-middle, X-inner.
// The slab starting at k_begin is contiguous in memory.
const uint64_t data_start = k_begin * static_cast<uint64_t>(nx * ny);
if (vd.concentration)
  append_array(rt, "Substance Concentration", 1,
               &conc[data_start], piece_cells, sizeof(real_t));
if (vd.gradient)
  append_array(rt, "Diffusion Gradient", 3,
               &grad[data_start * 3], piece_cells, sizeof(real_t));
```

#### `WriteDiffusionStepVtu()` — alternative VTK_VOXEL output

Produces `.vtu` + `.pvtu` files.  Each diffusion box is represented as a `VTK_VOXEL` cell (type 11) with eight explicit corner nodes. Data is stored as `CellData` — one value per voxel centre. This implementation is complete and correct but not active by default.

Corner node coordinates (no `+0.5` offset — these are corners, not centres):

```cpp
for (uint64_t kl = 0; kl <= k_len; ++kl)
  for (uint64_t j = 0; j <= ny; ++j)
    for (uint64_t i = 0; i <= nx; ++i) {
      pts[idx++] = dims[0] + i * box;
      pts[idx++] = dims[2] + j * box;
      pts[idx++] = dims[4] + (k_begin + kl) * box;
    }
```

VTK_VOXEL node ordering (type 11):

```
       6─────7          n0 = (i,   j,   k  )   n1 = (i+1, j,   k  )
      /|    /|          n2 = (i,   j+1, k  )   n3 = (i+1, j+1, k  )
     4─────5 |          n4 = (i,   j,   k+1)   n5 = (i+1, j,   k+1)
     | 2───|─3          n6 = (i,   j+1, k+1)   n7 = (i+1, j+1, k+1)
     |/    |/
     0─────1
```

The Y-axis nodes follow the same low-to-high ordering as X (not counterclockwise face winding as in `VTK_HEXAHEDRON`).  Getting this wrong causes degenerate cells.

---

## 6. Comparison: Standalone vs ParaView Adaptor

This table covers every significant dimension of the two implementations.

| Aspect | ParaView Adaptor | Standalone Adaptor |
|---|---|---|
| **External dependencies** | VTK, ParaView Catalyst, Qt (~500 MB) | C++ standard library only |
| **Build flag** | `-Dparaview=ON` (default) | `-Dstandalone_visualization=ON` |
| **Plugin library** | `libVisualizationAdaptor.so` (built from `src/core/visualization/paraview/`) | `libVisualizationAdaptor.so` (built from `src/core/visualization/standalone/`) |
| **Agent file format** | VTU (via VTK's `vtkUnstructuredGrid`) | VTU (hand-written XML + raw binary) |
| **Diffusion file format** | **VTI** (VTK Image Data — implicit structured grid) | **VTI** by default; **VTU** with `VTK_VOXEL` cells available as alternative |
| **Diffusion data placement** | `PointData` on `vtkImageData` (box centers as "points") | `PointData` at box centres (VTI default); `CellData` on `VTK_VOXEL` (VTU alternative) |
| **Parallel index format** | `.pvti` for diffusion, `.pvtu` for agents | `.pvti` for diffusion, `.pvtu` for agents |
| **ParaView volume rendering** | Native (VTI is a structured image, `vtkSmartVolumeMapper`) | Native (VTI with PointData, `vtkSmartVolumeMapper`) |
| **Parallelism mechanism** | OpenMP (via VTK's parallel writer) | OpenMP (each thread writes its own `.vtu` file) |
| **Data copy** | VTK arrays hold references to BioDynaMo's memory via `SetArray(..., 1)` (zero-copy) | Zero-copy for diffusion (pointer into DiffusionGrid's array); copy for agents (flat pre-extracted arrays) |
| **In-situ (live) visualization** | Supported via Catalyst pipeline | Not supported (export only) |
| **PVSM state file generation** | Yes (auto-generated Python Catalyst state) | Not generated |
| **bdm.toml adaptor name** | `adaptor = "paraview"` (default if not set) | `adaptor = "standalone"` |

### Why VTI for Diffusion?

Both adaptors use VTI (`.vti` files) for diffusion.  VTI is the most compact and efficient format for a uniform structured grid: the geometry is fully described by `Origin`, `Spacing`, and `Extent` — no coordinate arrays are stored at all.  VTK's rendering pipeline has a dedicated fast path for ImageData.

The standalone adaptor writes VTI by hand, using only `std::fstream`, without any VTK library dependency.

### Why `PointData` and Not `CellData` for the VTI Format?

`vtkSmartVolumeMapper` (ParaView's default volume renderer for structured grids) requires data in `PointData`.  The standalone VTI output stores concentration and gradient as `PointData` with `Origin` set to the first box centre and `Spacing` equal to the box length.  This means each VTK "node" sits exactly at a BioDynaMo box centre, so the physical meaning is preserved even though the VTK format calls it `PointData`.

The alternative VTU implementation uses `CellData` on `VTK_VOXEL` hexahedral cells, which is physically correct for a cell-centred finite-difference scheme, but requires `vtkUnstructuredGridVolumeMapper` instead of `vtkSmartVolumeMapper` for volume rendering.

---

## 7. VTK File Formats Explained

### 7.1 VTU (VTK XML Unstructured Grid)

```xml
<?xml version="1.0"?>
<VTKFile type="UnstructuredGrid" version="0.1"
         byte_order="LittleEndian" header_type="UInt32">
  <UnstructuredGrid>
    <Piece NumberOfPoints="6724" NumberOfCells="4800">

      <!-- CellData: one value per cell (voxel) -->
      <CellData>
        <DataArray type="Float64" Name="Substance Concentration"
                   NumberOfComponents="1" format="appended" offset="0"/>
      </CellData>

      <!-- Points: XYZ coordinates of all corner nodes -->
      <Points>
        <DataArray type="Float64" NumberOfComponents="3"
                   format="appended" offset="38404"/>
      </Points>

      <!-- Cells: topology (which nodes form which cells) -->
      <Cells>
        <DataArray type="UInt32" Name="connectivity"  offset="..."/>
        <DataArray type="UInt32" Name="offsets"       offset="..."/>
        <DataArray type="UInt8"  Name="types"         offset="..."/>
      </Cells>
    </Piece>
  </UnstructuredGrid>

  <!-- Raw binary block: each array preceded by a 4-byte length header -->
  <AppendedData encoding="raw">
  _[4-byte length][raw Float64 concentration data][4-byte length][raw Float64 coords]...
  </AppendedData>
</VTKFile>
```

The `offset` values in each `<DataArray>` tag are **byte offsets from the start of the binary block** (i.e., from the byte immediately after the `_` character). They let a reader seek directly to any array without parsing the whole file.

### 7.2 PVTU (Parallel VTU Index)

```xml
<VTKFile type="PUnstructuredGrid">
  <PUnstructuredGrid>
    <PCellData>
      <PDataArray type="Float64" Name="Substance Concentration" .../>
    </PCellData>
    <PPoints>
      <PDataArray type="Float64" NumberOfComponents="3"/>
    </PPoints>
    <!-- Each piece file is listed here -->
    <Piece Source="diffusion_Substance_0_10_p0.vtu"/>
    <Piece Source="diffusion_Substance_0_10_p1.vtu"/>
    ...
  </PUnstructuredGrid>
</VTKFile>
```

The PVTU file is a lightweight XML index. It tells ParaView which `.vtu` files form one complete dataset and what arrays they contain. ParaView reads the PVTU and loads all referenced pieces to reconstruct the full domain.

---

## 8. Output File Structure

```
output/{simulation_name}/viz/
├── agents_0.pvtu                      # step 0 agent index
├── agents_0_p0.vtu                    # step 0 agents, thread 0
├── agents_0_p1.vtu                    # step 0 agents, thread 1
│   …
├── agents_10.pvtu                     # step 10 agent index
├── agents_10_p0.vtu
│   …
├── diffusion_Substance_0_0.pvti       # step 0 diffusion index for Substance_0
├── diffusion_Substance_0_0_p0.vti     # step 0 diffusion, Z-slab 0
├── diffusion_Substance_0_0_p1.vti     # step 0 diffusion, Z-slab 1
│   …
└── diffusion_Substance_0_10.pvti      # step 10 diffusion index
    …
```

To open in ParaView: **File → Open** the `.pvtu` files for agents and the `.pvti` files for diffusion. ParaView will automatically load all referenced piece files. Use the `Colour by` dropdown to select `Substance Concentration` or `Diffusion Gradient`. To volume-render the diffusion, change `Representation` from `Surface` to `Volume`.

---

## 9. How to Build and Enable

```bash
# Standalone exporter (no ParaView needed):
cmake -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -Dparaview=OFF \
      -Dstandalone_visualization=ON \
      -B build
cmake --build build --parallel

# Source the environment before running any simulation:
source build/bin/thisbdm.sh
```

To build with ParaView (the default, unchanged path):

```bash
cmake -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -Dparaview=ON \
      -B build-paraview
# Note: -Dstandalone_visualization must NOT be ON when using paraview
```

---

## 10. Configuration Reference

Add the following to your simulation's `bdm.toml` to use the standalone exporter:

```toml
[visualization]
export   = true          # must be true to write any files
interval = 10            # write every 10 simulation steps
adaptor  = "standalone"  # select this adaptor (default is "paraview")

# Export agent positions and attributes:
[[visualize_agent]]
name = "MyCell"                                     # C++ class name of your agent
additional_data_members = ["my_field_", "score_"]   # extra scalar members to export

# Export a diffusion substance:
[[visualize_diffusion]]
name          = "Substance_0"   # name passed to ModelInitializer::DefineSubstance
concentration = true            # export the scalar concentration field
gradient      = true            # export the 3-component gradient vector field
```

---

## 11. Changes to Existing Files

In addition to the entirely new `src/core/visualization/standalone/` directory, the following existing files were modified:

### `src/core/visualization/visualization_adaptor.cc`

Two additions:

1. **`#include "TEnv.h"` and explicit `bdm.rootrc` loading:** Ensures ROOT's plugin manager can find `libVisualizationAdaptor.so` even when the environment is not fully set up before the first `Visualize()` call.

2. **Programmatic `AddHandler` for `"standalone"`:** Registers the standalone plugin handler in C++ rather than relying on ROOT to parse the `.C` macro file, because cling's macro parser sometimes fails on certain Linux configurations.

### `etc/plugins/VisualizationAdaptor/P000_StandaloneAdaptor.C`

Replaced a malformed file (missing closing `}`) with a minimal valid empty function. ROOT must be able to parse this file without errors when scanning the plugin directory; the actual handler registration is handled programmatically as described above.

### `CMakeLists.txt`

See [Section 3](#3-cmake-changes) for the complete annotated diff.
