# Visualization Pipeline: Call-Chain, Design Rationale, and File Formats

This document answers three questions in depth:

1. **How does visualization work end-to-end?**  
   Step-by-step call chains from `Scheduler::Simulate()` all the way to bytes on disk — for both the ParaView path and the Standalone path.

2. **Why is the code structured this way?**  
   The design patterns (Strategy, Plugin, Lazy Initialization) that make the two paths interchangeable without recompiling the simulation.

3. **What are VTU, PVTU, VTI, PVTI files, and why are they written the way they are?**  
   A detailed walkthrough of the VTK XML file formats, the appended-binary layout, and the choices behind them.

---

## Table of Contents

1. [Architecture overview](#1-architecture-overview)
2. [Design patterns](#2-design-patterns)
3. [Step 1: Scheduler — where visualization begins](#3-step-1-scheduler--where-visualization-begins)
4. [Step 2: VisualizationOp — the operation bridge](#4-step-2-visualizationop--the-operation-bridge)
5. [Step 3: VisualizationAdaptor::Create — plugin loading](#5-step-3-visualizationadaptorcreate--plugin-loading)
6. [Step 4a — ParaView path: full call chain](#6-step-4a--paraview-path-full-call-chain)
7. [Step 4b — Standalone path: full call chain](#7-step-4b--standalone-path-full-call-chain)
8. [Side-by-side comparison](#8-side-by-side-comparison)
9. [VTK file formats in depth](#9-vtk-file-formats-in-depth)
10. [Why appended raw binary?](#10-why-appended-raw-binary)
11. [VTU file anatomy (agents)](#11-vtu-file-anatomy-agents)
12. [PVTU file anatomy](#12-pvtu-file-anatomy)
13. [VTI vs VTU for diffusion](#13-vti-vs-vtu-for-diffusion)
14. [VTK cell types used by BioDynaMo](#14-vtk-cell-types-used-by-biodynamo)
15. [Glossary](#15-glossary)

---

## 1. Architecture overview

```
User simulation code
        │
        ▼
  Simulation::Simulate(N)
        │
        ▼
   Scheduler::Execute()   ← runs every step
        │
        ├── RunPreScheduledOps()
        ├── RunScheduledOps()
        │       └── VisualizationOp::operator()()   ← scheduled post-op
        └── RunPostScheduledOps()
                    │
                    ▼
        VisualizationAdaptor::Visualize()   ← pure virtual call
              /                    \
    ParaviewAdaptor            StandaloneAdaptor
     ::Visualize()              ::Visualize()
           │                          │
    VTK in-memory              StandaloneExporter
    pipeline + writers          ::WriteStep()
    (vtkUnstructuredGrid,       ::WriteDiffusionStep()
     vtkImageData, etc.)              │
           │                    writes agents: .vtu/.pvtu
    writes .vtu/.pvtu             diffusion: .vti/.pvti
    via vtkXMLWriter*             directly via std::fstream
    (Catalyst pipeline)        (no VTK dependency)
```

Both paths produce compatible VTK XML files.  The difference is **who manages the VTK data structures**: the ParaView path builds real VTK in-memory objects and lets the VTK library write them; the Standalone path serializes the same format by hand using only the C++ standard library.

---

## 2. Design patterns

### 2.1 Strategy pattern

`VisualizationAdaptor` is the *Strategy* interface:

```cpp
// src/core/visualization/visualization_adaptor.h
class VisualizationAdaptor {
 public:
  static VisualizationAdaptor* Create(const std::string& adaptor);
  virtual void Visualize() = 0;     // ← the one strategy method
};
```

`VisualizationOp` is the *Context* that holds a pointer to whatever concrete strategy was loaded at runtime.  The simulation code never mentions `ParaviewAdaptor` or `StandaloneAdaptor` by name — it calls `visualization_->Visualize()` through the abstract pointer.

**Why?** Because the correct adaptor can only be determined at runtime (it depends on the `visualization_engine` key in `bdm.toml` and on which shared libraries are installed on the machine), not at compile time.

### 2.2 Plugin pattern (ROOT TPluginManager)

The concrete adaptors live in a separately compiled shared library (`libVisualizationAdaptor.so`).  ROOT's `TPluginManager` is the *Factory Registry* that maps the string `"paraview"` or `"standalone"` to a `Factory()` function in that library.

The registration lives in `$BDMSYS/etc/bdm.rootrc`:

```
Plugin.VisualizationAdaptor:  paraview   bdm::ParaviewAdaptor   VisualizationAdaptor  Factory()
Plugin.VisualizationAdaptor:  standalone bdm::StandaloneAdaptor VisualizationAdaptor  Factory()
```

When `VisualizationAdaptor::Create("standalone")` is called:

```
gPluginMgr->FindHandler("VisualizationAdaptor", "standalone")
   → TPluginHandler::LoadPlugin()           // dlopen(libVisualizationAdaptor.so)
   → TPluginHandler::ExecPlugin(0)          // calls bdm::StandaloneAdaptor::Factory()
   → returns (VisualizationAdaptor*)ptr
```

**Why a plugin?** Because the ParaView shared libraries are large (hundreds of MB) and optional.  A simulation compiled without ParaView must still link and run — it just calls `dlopen` on a library that happens to not need VTK.  The plugin mechanism is the standard ROOT way to make optional components available at runtime without a hard link-time dependency.

### 2.3 Lazy initialization

Both adaptors defer expensive setup (directory creation, VTK object allocation, ROOT configuration) to the first `Visualize()` call via an `initialized_` flag:

```cpp
void StandaloneAdaptor::Visualize() {
  if (!initialized_) {
    // Create output directory, allocate exporter — only once.
    initialized_ = true;
  }
  // ... I/O gate check ...
}
```

**Why?** The `Simulation` object (which provides the output directory path) is not fully initialized when the adaptor is constructed.  The scheduler creates adaptors during its own constructor, before the simulation output directory has been created.  Deferring to the first `Visualize()` call guarantees the directory is ready.

### 2.4 I/O gate (interval guard)

Both adaptors check two conditions before writing any file:

```cpp
if (param->export_visualization &&
    (total_steps % param->visualization_interval == 0))
```

This mirrors the pattern across both implementations.  The scheduler calls `VisualizationOp` every step; the adaptor decides whether the step is worth exporting.  This avoids building file-write logic into the scheduler itself.

---

## 3. Step 1: Scheduler — where visualization begins

```
src/core/scheduler.cc
```

### Constructor

```cpp
Scheduler::Scheduler() {
  // ...
  std::vector<std::string> post_scheduled_ops_names = {
      "load balancing", "tear down iteration", "update environment",
      "visualize",       // ← visualization is a post-scheduled standalone op
      "update time series"
  };
  // All ops in post_scheduled_ops_names are registered via ScheduleOp().
  // After all ops are registered, VisualizationOp::Initialize() is called:
  GetOps("visualize")[0]
      ->GetImplementation<VisualizationOp>()
      ->Initialize();
}
```

The `"visualize"` name resolves through the `OperationRegistry` (a compile-time `BDM_REGISTER_OP` macro) to `VisualizationOp`.

### Simulate loop

```cpp
void Scheduler::Simulate(uint64_t steps) {
  Initialize(steps);
  for (unsigned step = 0; step < steps; step++) {
    Execute();         // ← called N times
    total_steps_++;
    UpdateSimulatedTime();
    Backup();
  }
}
```

### Execute

```cpp
void Scheduler::Execute() {
  ScheduleOps();
  RunPreScheduledOps();   // "set up iteration", "propagate staticness"
  RunScheduledOps();      // agent ops + standalone ops (behaviors, mechanics, diffusion…)
  RunPostScheduledOps();  // "load balancing", "visualize", "update time series"
}
```

The visualization operation runs **after** all physics for the step — agents have moved, diffusion has diffused, so the snapshot written to disk reflects the final state of that step.

---

## 4. Step 2: VisualizationOp — the operation bridge

```
src/core/operation/visualization_op.h
```

```cpp
class VisualizationOp : public StandaloneOperationImpl {
 public:
  void Initialize() {
    auto* param = Simulation::GetActive()->GetParam();
    // param->visualization_engine is read from bdm.toml:
    //   [visualization]
    //   adaptor = "standalone"   (or "paraview")
    visualization_ = VisualizationAdaptor::Create(param->visualization_engine);
  }

  void operator()() override {
    if (!initialized_) {
      Initialize();         // lazy safety net (normally done in Scheduler ctor)
    }
    if (visualization_ != nullptr) {
      visualization_->Visualize();   // ← polymorphic dispatch
    }
  }

 private:
  VisualizationAdaptor* visualization_ = nullptr;
};
```

`StandaloneOperationImpl` means this operation runs once per step on the whole simulation, not once per agent.  It is called in `RunPostScheduledOps()` after all agent work is done.

---

## 5. Step 3: VisualizationAdaptor::Create — plugin loading

```
src/core/visualization/visualization_adaptor.cc
```

```cpp
VisualizationAdaptor* VisualizationAdaptor::Create(const std::string& adaptor) {
  // 1. Return nullptr immediately if visualization is disabled in bdm.toml.
  if (!(param->insitu_visualization || param->export_visualization))
    return nullptr;

  // 2. Load bdm.rootrc so ROOT knows where the plugin shared library is.
  gEnv->ReadFile("$BDMSYS/etc/bdm.rootrc", kEnvUser);

  // 3. Ask ROOT's plugin manager for a handler matching the adaptor name.
  auto* h = gPluginMgr->FindHandler("VisualizationAdaptor", adaptor.c_str());

  // 3a. Fallback: if "standalone" was not found via bdm.rootrc (e.g. the .C
  //     file failed to parse), register the handler programmatically.
  if (!h && adaptor == "standalone") {
    gPluginMgr->AddHandler("VisualizationAdaptor", "standalone",
                           "bdm::StandaloneAdaptor", "VisualizationAdaptor",
                           "Factory()");
    h = gPluginMgr->FindHandler("VisualizationAdaptor", adaptor.c_str());
  }

  // 4. Load the shared library (dlopen) and call Factory().
  h->LoadPlugin();    // dlopen(libVisualizationAdaptor.so)
  return reinterpret_cast<VisualizationAdaptor*>(h->ExecPlugin(0));
  //                                              ^-- calls Factory()
}
```

The `loaded_` static map caches plugin handlers so that `LoadPlugin()` is only called once per adaptor type per simulation run.  Subsequent steps call `ExecPlugin(0)` directly (which calls `Factory()` again to get a new object — but the shared library is already in memory).

---

## 6. Step 4a — ParaView path: full call chain

```
src/core/visualization/paraview/adaptor.cc
src/core/visualization/paraview/vtk_agents.cc
src/core/visualization/paraview/vtk_diffusion_grid.cc
src/core/visualization/paraview/parallel_vtu_writer.cc
src/core/visualization/paraview/parallel_vti_writer.cc
```

### 6.1 ParaviewAdaptor::Visualize()

```
ParaviewAdaptor::Visualize()
  │
  ├── [step 0, first call only] Initialize()
  │       ├── vtkCPProcessor::New() + Initialize()     ← Catalyst in-situ engine
  │       ├── for each visualize_agents entry:
  │       │       VtkAgents(name, data_description_)   ← allocate VTK containers
  │       └── for each visualize_diffusion entry:
  │               VtkDiffusionGrid(name, data_description_)
  │
  ├── [interval check] total_steps % visualization_interval != 0  → return
  │
  ├── CreateVtkObjects()
  │       ├── BuildAgentsVTKStructures()
  │       │       └── for each agent type:
  │       │               VtkAgents::Update(agents)
  │       │                   └── UpdateMappedDataArrays(tid, agents, start, end)
  │       │                       └── reads agent fields via MappedDataArray
  │       │                           (zero-copy: maps VTK arrays directly to
  │       │                            agent memory via TDataMember::GetOffset)
  │       └── BuildDiffusionGridVTKStructures()
  │               └── for each diffusion grid:
  │                       VtkDiffusionGrid::Update(grid)
  │                           └── copies concentration/gradient from DiffusionGrid
  │                               into vtkImageData arrays
  │
  ├── [if insitu] InsituVisualization()
  │       └── vtkCPProcessor::CoProcess(data_description_)
  │           (sends data to a live ParaView server over a socket)
  │
  └── [if export] ExportVisualization()
          ├── WriteSimulationInfoJsonFile()  ← generates simulation_info.json once
          ├── for each VtkAgents:
          │       VtkAgents::WriteToFile(step)
          │           └── ParallelVtuWriter::operator()(folder, prefix, grids)
          │               ├── for each grid (one per OpenMP thread):
          │               │       vtkXMLUnstructuredGridWriter → file_p{n}.vtu
          │               └── vtkXMLPUnstructuredGridWriter   → file.pvtu
          └── for each VtkDiffusionGrid:
                  VtkDiffusionGrid::WriteToFile(step)
                      └── ParallelVtiWriter::operator()(folder, prefix, images, …)
                          ├── for each image (one per Z-slab):
                          │       VtiWriter (subclass of vtkXMLImageDataWriter) → file_p{n}.vti
                          └── PvtiWriter::Write(…)  → file.pvti
```

### 6.2 Key data structures

| Object | Type | What it holds |
|--------|------|---------------|
| `VtkAgents::data_[tid]` | `vtkUnstructuredGrid*` | One unstructured grid per thread containing agent positions and attributes |
| `MappedDataArray<T>` | `vtkAOSDataArrayTemplate<T>` | Zero-copy view into agent memory — no data is copied, VTK reads directly |
| `VtkDiffusionGrid::data_[piece]` | `vtkImageData*` | One image data object per Z-slab containing concentration/gradient |

---

## 7. Step 4b — Standalone path: full call chain

```
src/core/visualization/standalone/adaptor.cc
src/core/visualization/standalone/standalone_exporter.cc
```

### 7.1 StandaloneAdaptor::Visualize()

```
StandaloneAdaptor::Visualize()
  │
  ├── [first call only] lazy init:
  │       std::filesystem::create_directories(output_dir + "/viz")
  │       exporter_ = new StandaloneExporter(output_dir + "/viz")
  │
  ├── [gate] !param->export_visualization  → return
  ├── [gate] total_steps % visualization_interval != 0  → return
  │
  ├── exporter_->WriteStep()
  │       └── [see §7.2]
  │
  └── exporter_->WriteDiffusionStep()
              └── [see §7.3]
```

### 7.2 StandaloneExporter::WriteStep()

```
WriteStep()
  │
  ├── Phase 1: Collect agent data
  │       rm->ForEachAgent([](Agent* a) {
  │           positions.push_back(a->GetPosition());
  │           ... });
  │       // All agents collected into flat vectors on a single pass.
  │
  ├── Phase 2: Extract Diameter into flat array (main thread, no races)
  │
  ├── Phase 3: ROOT reflection for extra user-defined fields
  │       extra_members_ was loaded from bdm.toml once in the constructor
  │       for each name in extra_members_:
  │           TClass* cls = TClass::GetClass(typeid(*agent))
  │           // Resolves the actual derived type at runtime (not Agent*).
  │           TDataMember* dm = cls->GetDataMember(name)
  │           size_t offset = dm->GetOffset()
  │           value = *reinterpret_cast<float*>((char*)agent + offset)
  │
  ├── Phase 4: Partition agents across threads
  │       piece_size = ceil(num_agents / num_threads)
  │
  ├── Phase 5: OMP parallel — each thread writes one .vtu piece file
  │       #pragma omp parallel for
  │       for p in [0, num_threads):
  │           5a. open  agents_{step}_p{p}.vtu
  │           5b. write VTK XML header (<?xml …>  <VTKFile …>)
  │           5c. write metadata table:
  │               append_array("Cell_ID",        offset) → <DataArray …/>
  │               append_array("Diameter",       offset)
  │               for each name in extra_members_:
  │                   append_array(name, offset)
  │               append_array("Points",         offset)
  │           5d. write <PointData> declarations (all arrays listed)
  │           5e. write <Points> declaration
  │           5f. write VTK_VERTEX topology (connectivity + offsets + types)
  │           5g. write binary AppendedData block:
  │               for each array:
  │                   uint32_t byte_length
  │                   raw bytes (floats / int32 / int64)
  │           close </AppendedData> </UnstructuredGrid> </VTKFile>
  │
  └── Phase 6: (main thread) WritePvtu(num_pieces)
              step_++
```

### 7.3 StandaloneExporter::WriteDiffusionStep()

`WriteDiffusionStep()` is a thin dispatcher — it calls `WriteDiffusionStepVti()` (the default) and then increments `step_`.  A complete `WriteDiffusionStepVtu()` implementation also exists and can be activated by replacing the call inside `WriteDiffusionStep()`.

#### Default path: `WriteDiffusionStepVti()` (VTK ImageData)

```
WriteDiffusionStepVti()
  │
  ├── for each substance in param->visualize_diffusion:
  │       DiffusionGrid* grid = rm->GetDiffusionGrid(name)
  │       nx, ny, nz = grid->GetNumBoxesArray()
  │       box        = grid->GetBoxLength()
  │       dims       = grid->GetDimensions()   // {xmin,xmax,ymin,ymax,zmin,zmax}
  │       conc_ptr   = grid->GetAllConcentrations()  // zero-copy pointer
  │       grad_ptr   = grid->GetAllGradients()       // zero-copy pointer
  │
  │       origin = (dims[0]+0.5*box, dims[2]+0.5*box, dims[4]+0.5*box)
  │       // Origin at first box centre so VTK "nodes" = BioDynaMo box centres
  │
  │       Partition: boxes_per_piece = ceil(nz / num_threads)
  │
  │       #pragma omp parallel for
  │       for p in [0, num_pieces):
  │           k_begin = p * boxes_per_piece
  │           k_len   = min(boxes_per_piece, nz - k_begin)
  │
  │           // Non-last pieces: k_end = k_begin + k_len (shared boundary node)
  │           // Last piece:      k_end = nz - 1
  │           // layers = k_len+1 (non-last) or k_len (last)
  │
  │           open  diffusion_{name}_{step}_p{p}.vti
  │           write XML header (ImageData, WholeExtent="0 nx-1 0 ny-1 0 nz-1")
  │           write Piece Extent="0 nx-1 0 ny-1 k_begin k_end"
  │           write <PointData> declarations (concentration, gradient)
  │           write binary AppendedData (zero-copy from grid arrays)
  │           close file
  │
  └── WriteDiffusionPvti(name, has_conc, has_grad, nx, ny, nz,
                         ox, oy, oz, box, num_pieces, boxes_per_piece)
```

#### Alternative path: `WriteDiffusionStepVtu()` (VTK_VOXEL UnstructuredGrid)

```
WriteDiffusionStepVtu()
  │
  ├── for each substance in param->visualize_diffusion:
  │       [same grid setup as VTI path]
  │
  │       #pragma omp parallel for
  │       for p in [0, num_pieces):
  │           k_begin, k_len [same partitioning]
  │           num_nodes = (nx+1)*(ny+1)*(k_len+1)
  │           num_cells = nx*ny*k_len
  │
  │           build corner-node coords:
  │               for kl in [0, k_len], j in [0, ny], i in [0, nx]:
  │                   pt = (dims[0]+i*box, dims[2]+j*box, dims[4]+(k_begin+kl)*box)
  │
  │           build VTK_VOXEL connectivity (8 nodes per cell):
  │               n0=(i,j,k) n1=(i+1,j,k) n2=(i,j+1,k) n3=(i+1,j+1,k)
  │               n4=(i,j,k+1) n5=(i+1,j,k+1) n6=(i,j+1,k+1) n7=(i+1,j+1,k+1)
  │
  │           open  diffusion_{name}_{step}_p{p}.vtu
  │           write XML header (UnstructuredGrid)
  │           write <CellData> declarations (concentration, gradient)
  │           write binary AppendedData (CellData + corner Points + topology)
  │           close file
  │
  └── WriteDiffusionVtuIndex(name, has_conc, has_grad, num_pieces)
```

---

## 8. Side-by-side comparison

| Aspect | ParaView path | Standalone path |
|--------|---------------|-----------------|
| **Dependency** | VTK + ParaView (hundreds of MB) | C++ stdlib + ROOT reflection only |
| **Agent data structure** | `vtkUnstructuredGrid` (in-memory VTK object) | `std::vector<float>` flat arrays |
| **Agent memory access** | `MappedDataArray` — zero-copy VTK view into agent memory | `ForEachAgent` loop — copies into vectors |
| **Diffusion data structure** | `vtkImageData` (in-memory VTK object) | Pointer directly into `DiffusionGrid` memory |
| **File writing** | `vtkXMLUnstructuredGridWriter`, `vtkXMLImageDataWriter` | `std::fstream` + manual VTK XML serialization |
| **Agent file format** | VTU (Unstructured Grid) | VTU (Unstructured Grid) |
| **Diffusion file format** | VTI (Image Data — uniform structured grid) | VTI (Image Data) by default; VTU with VTK_VOXEL cells available as alternative |
| **Diffusion layout** | `PointData` at box centres | `PointData` at box centres (VTI default); `CellData` at voxel centres (VTU alternative) |
| **Parallelism** | VTK internal parallelism via `ParallelVtuWriter` / `ParallelVtiWriter` | OpenMP parallel for loop |
| **In-situ support** | Yes (Catalyst pipeline via `vtkCPProcessor`) | No (export-only) |
| **PVSM generation** | Yes (`generate_pv_state.py` via `pvbatch`) | No |
| **Volume rendering in ParaView** | Works out of the box (`vtkSmartVolumeMapper` on VTI PointData) | Works out of the box (`vtkSmartVolumeMapper` on VTI PointData) |

### Why does diffusion use VTI for both paths?

VTI (Image Data) is the most compact and efficient format for a uniform structured grid: the geometry is fully described by `Origin`, `Spacing`, and `Extent` — no explicit coordinate arrays are stored.  VTK's rendering pipeline and `vtkSmartVolumeMapper` have a dedicated fast path for ImageData with `PointData`.

Both adaptors set `Origin` at the first box centre and `Spacing` equal to the box length, so each VTK "node" coincides with a BioDynaMo box centre.  The difference is that the ParaView adaptor uses `vtkImageData` objects and VTK's own writer, while the standalone adaptor serializes the same XML format by hand with only `std::fstream`.

The standalone adaptor also keeps a `WriteDiffusionStepVtu()` implementation (VTK_VOXEL cells, `CellData`) as an alternative.  It is physically correct — `CellData` with one value per voxel centre matches BioDynaMo's cell-centred finite-difference scheme — but requires `vtkUnstructuredGridVolumeMapper` instead of `vtkSmartVolumeMapper` for volume rendering.

---

## 9. VTK file formats in depth

VTK XML files come in two flavors:

- **Serial** (`.vtu`, `.vti`) — contains all data for a single process/piece.
- **Parallel** (`.pvtu`, `.pvti`) — an index-only XML file that lists the serial piece files without copying their data.

ParaView loads the `.pvtu` / `.pvti` file and then reads the individual pieces, potentially on different nodes, to assemble the full dataset.

### 9.1 VTU — Unstructured Grid

A VTU file describes a mesh where every cell is explicitly defined: you must list point coordinates, a connectivity array (which points form each cell), an offsets array (cumulative cell sizes), and a types array (cell shape code).

This makes VTU general — it can represent any mesh topology — but it costs more storage than structured formats because every cell's geometry is explicit.

### 9.2 VTI — Image Data

A VTI file describes a regular structured grid (a lattice of identical rectangular boxes).  The geometry is fully determined by three numbers: origin, spacing, and extent.  You never store explicit point coordinates.

VTI is ideal for diffusion grids because BioDynaMo's `DiffusionGrid` is exactly a uniform Cartesian grid.  The VTI overhead per file is constant regardless of grid size.

### 9.3 Data encoding: ASCII vs Binary vs Appended Binary

VTK XML supports three encodings for numerical data:

| Encoding | What is stored in the XML | Pros | Cons |
|----------|--------------------------|------|------|
| `ascii` | Numbers as text inside `<DataArray>` | Human-readable, debuggable | ~5-10× larger files; slow to write and read |
| `binary` | Base64-encoded data inside `<DataArray>` | All data in one XML pass | Base64 inflates size by ~33% |
| `appended` | Offsets in the XML; raw bytes after `</UnstructuredGrid>` | Compact; I/O-optimal | More complex to write |

BioDynaMo uses **appended raw binary** for both adaptors, which is the production-quality choice.

---

## 10. Why appended raw binary?

### 10.1 The offset mechanism

In appended mode, the `<DataArray>` elements in the XML body do not contain data — they only contain an `offset` attribute pointing to a byte position in the binary block:

```xml
<PointData>
  <DataArray Name="Diameter" type="Float32" format="appended" offset="0"/>
  <DataArray Name="Mass"     type="Float32" format="appended" offset="404"/>
</PointData>
...
<AppendedData encoding="raw">
  _[4-byte length][raw floats for Diameter][4-byte length][raw floats for Mass]...
```

The 4-byte `uint32` length prefix before each array is required by the VTK spec: it tells the reader exactly how many bytes to consume for each array before moving to the next.

### 10.2 Why this is optimal

**Sequential I/O.** The binary block is written once, in one sequential pass to disk.  There is no interleaving of XML and data bytes, which maximizes OS write-ahead buffering.

**No encoding overhead.** Raw bytes means float32 values are written as 4 bytes each — nothing more.  For a simulation with 10⁶ agents and 5 float attributes, that is 20 MB per step instead of ~100 MB for ASCII or ~27 MB for Base64.

**Random access by the reader.** The offset mechanism lets ParaView seek directly to any array without reading preceding arrays.  If a user only wants to colour by `Diameter` and not `Mass`, ParaView can seek to `offset=0` and read 404 bytes, skipping `Mass` entirely.

**Two-pass write strategy.** The Standalone adaptor exploits this by writing the XML header first (with placeholder offsets computed from array sizes), then appending the binary block.  The XML pass is cheap (small text); the binary pass is fast (sequential floats).

### 10.3 Two-pass write: how the standalone adaptor does it

```
Step 1: Compute byte layout before opening any file.
        For each array, compute:
            byte_offset += sizeof(uint32)   (length prefix)
            byte_offset += num_elements * sizeof(float)

Step 2: Write the XML header.
        Use the precomputed offsets in <DataArray offset="N"/> attributes.
        This is the "metadata table" assembled by the append_array lambda.

Step 3: Write the binary block.
        Write the sentinel "_" byte (required by VTK spec).
        For each array in the same order:
            fwrite(&byte_length, 4, 1, file)    // length prefix
            fwrite(data_ptr, 1, byte_length, file)
```

The XML and binary sections must list arrays in the same order, because the reader uses the XML offsets to seek into the binary block.

---

## 11. VTU file anatomy (agents)

A complete minimal VTU file written by the Standalone adaptor looks like this (whitespace added for clarity):

```xml
<?xml version="1.0"?>
<VTKFile type="UnstructuredGrid" version="0.1" byte_order="LittleEndian">
  <UnstructuredGrid>
    <Piece NumberOfPoints="3" NumberOfCells="3">

      <!-- PointData: per-agent scalar/vector attributes -->
      <!-- Cell_ID and Diameter are always written.             -->
      <!-- Additional fields come from additional_data_members  -->
      <!-- in bdm.toml (e.g. "my_field_", "cell_type_").       -->
      <PointData>
        <DataArray Name="Cell_ID"  type="UInt64"  NumberOfComponents="1"
                   format="appended" offset="0"/>
        <DataArray Name="Diameter" type="Float64" NumberOfComponents="1"
                   format="appended" offset="20"/>
        <!-- extra_members_ fields follow here -->
      </PointData>

      <!-- Points: XYZ coordinates, one per agent -->
      <Points>
        <DataArray type="Float32" NumberOfComponents="3"
                   format="appended" offset="112"/>
      </Points>

      <!-- Cells: VTK topology arrays -->
      <!-- connectivity: which point indices form each cell -->
      <Cells>
        <DataArray Name="connectivity" type="Int32" format="appended" offset="148"/>
        <!-- offsets: cumulative count of points per cell -->
        <DataArray Name="offsets"      type="Int32" format="appended" offset="164"/>
        <!-- types: VTK cell type code for each cell -->
        <DataArray Name="types"        type="UInt8" format="appended" offset="180"/>
      </Cells>

    </Piece>
  </UnstructuredGrid>

  <!-- AppendedData: raw binary block, one uint32 length + bytes per array -->
  <AppendedData encoding="raw">
    _[length=24][int64×3: Cell_IDs]
     [length=12][float32×3: Diameters]
     ...
  </AppendedData>
</VTKFile>
```

### Why PointData and not CellData for agents?

In VTK's data model, `PointData` arrays have one value per point (vertex) and `CellData` arrays have one value per cell.

For agents represented as `VTK_VERTEX` (one-point cells), the distinction collapses: every "cell" is a single point, so `PointData` and `CellData` would contain the same number of values and ParaView would display them identically.

`PointData` is the conventional choice for per-agent attributes because:

1. It is consistent with how ParaView's own writers handle particle data.
2. It allows direct use of the `MappedDataArray` zero-copy mechanism in the ParaView path (which uses `PointData`).
3. It avoids a confusing mismatch where "CellData" refers to data about simulation agents rather than grid cells.

---

## 12. PVTU file anatomy

A `.pvtu` file is a lightweight XML index that lists all piece files and mirrors their data schema:

```xml
<?xml version="1.0"?>
<VTKFile type="PUnstructuredGrid" version="0.1" byte_order="LittleEndian">
  <PUnstructuredGrid GhostLevel="0">

    <!-- Schema mirror: same arrays as the individual .vtu pieces -->
    <!-- ParaView needs this to know what fields exist without loading pieces -->
    <PPointData>
      <PDataArray Name="Cell_ID"  type="UInt64"  NumberOfComponents="1"/>
      <PDataArray Name="Diameter" type="Float64" NumberOfComponents="1"/>
      <!-- extra_members_ fields mirrored here -->
    </PPointData>
    <PPoints>
      <PDataArray type="Float32" NumberOfComponents="3"/>
    </PPoints>

    <!-- Piece list: relative paths to individual .vtu files -->
    <Piece Source="agents_0_p0.vtu"/>
    <Piece Source="agents_0_p1.vtu"/>
    <Piece Source="agents_0_p2.vtu"/>
    <Piece Source="agents_0_p3.vtu"/>

  </PUnstructuredGrid>
</VTKFile>
```

### Why does the schema need to be mirrored?

ParaView reads the `.pvtu` file first to decide what fields are available, so that it can populate the field selector UI before loading any piece.  If the schema was absent from the `.pvtu`, ParaView would have to load at least one piece to discover available fields.

The `PDataArray` elements in `<PPointData>` carry the same `Name`, `type`, and `NumberOfComponents` attributes as the per-piece `<DataArray>` elements, but no `offset` or `format` — those are piece-local details.

---

## 13. VTI for diffusion

Both the ParaView adaptor and the standalone adaptor write diffusion as **VTI** (VTK ImageData).  The standalone adaptor also keeps a VTU/VTK_VOXEL implementation as an available alternative.

### 13.1 VTI — default format for both paths

```xml
<VTKFile type="ImageData" …>
  <ImageData WholeExtent="0 39 0 39 0 39"
             Origin="10 10 10"
             Spacing="20.0 20.0 20.0">
    <!-- Origin at first box centre; Spacing = box length.
         WholeExtent covers N box centres per dimension (not N+1 nodes). -->
    <Piece Extent="0 39 0 39 0 5">   ← this piece covers Z slabs 0–5
      <PointData>
        <DataArray Name="Substance Concentration" … format="appended" offset="0"/>
        <DataArray Name="Diffusion Gradient"      … format="appended" offset="…"/>
      </PointData>
      <!-- No <Points> or <Cells> — fully determined by Origin/Spacing/Extent -->
    </Piece>
  </ImageData>
  <AppendedData encoding="raw">…</AppendedData>
</VTKFile>
```

Key parameters:
- `Origin` is placed at the **first box centre** (`dims[i] + 0.5 * box`), not at the grid corner.
- `WholeExtent` uses N indices per dimension (one per box centre), not N+1.
- `PointData` holds one value per VTK "node" — each node corresponds to a BioDynaMo box centre.
- Adjacent pieces share one boundary node (`vtkXMLPImageDataReader` requires this for gap-free assembly).

This layout satisfies two requirements simultaneously: `vtkSmartVolumeMapper` finds the data in `PointData`, and the shared-boundary convention prevents gaps between pieces.

### 13.2 VTU with VTK_VOXEL — standalone alternative

```xml
<VTKFile type="UnstructuredGrid" …>
  <UnstructuredGrid>
    <Piece NumberOfPoints="(nx+1)*(ny+1)*(nz_slab+1)"
           NumberOfCells="nx*ny*nz_slab">
      <CellData>
        <DataArray Name="Substance Concentration" type="Float64" NumberOfComponents="1"
                   format="appended" offset="0"/>
      </CellData>
      <Points>
        <DataArray type="Float64" NumberOfComponents="3"
                   format="appended" offset="…"/>
      </Points>
      <Cells>
        <DataArray Name="connectivity" type="Int32" format="appended" offset="…"/>
        <DataArray Name="offsets"      type="Int32" format="appended" offset="…"/>
        <DataArray Name="types"        type="UInt8" format="appended" offset="…"/>
      </Cells>
    </Piece>
  </UnstructuredGrid>
  <AppendedData encoding="raw">…</AppendedData>
</VTKFile>
```

`CellData` has one value per voxel centre — physically correct for BioDynaMo's cell-centred finite-difference scheme.  To activate, replace `WriteDiffusionStepVti()` with `WriteDiffusionStepVtu()` inside `WriteDiffusionStep()`.

### 13.3 VTK_VOXEL node ordering

VTK_VOXEL is an axis-aligned hexahedron with a specific vertex ordering defined by the VTK spec.  Getting the order wrong produces degenerate cells that ParaView cannot render.

```
        6───────7
       /|      /|
      4───────5 |
      | |     | |
      | 2─────|─3
      |/      |/
      0───────1

n0 = (i,   j,   k  )   n1 = (i+1, j,   k  )
n2 = (i,   j+1, k  )   n3 = (i+1, j+1, k  )
n4 = (i,   j,   k+1)   n5 = (i+1, j,   k+1)
n6 = (i,   j+1, k+1)   n7 = (i+1, j+1, k+1)
```

This differs from `VTK_HEXAHEDRON` (type 12), which uses counterclockwise face winding.  `VTK_VOXEL` is strictly axis-aligned: n0 and n3 must be diagonally opposite on the bottom face, n0 and n7 diagonally opposite in 3D.

---

## 14. VTK cell types used by BioDynaMo

| Type code | Name | Dimensions | Used for |
|-----------|------|-----------|---------|
| `1` | `VTK_VERTEX` | 0D (point) | Agent positions (both paths) |
| `11` | `VTK_VOXEL` | 3D (axis-aligned hexahedron) | Diffusion voxels (Standalone VTU alternative) |
| *(implicit)* | Image Data grid | 3D | Diffusion voxels (both paths default — VTI, no explicit cell type) |

`VTK_VERTEX` (type 1) represents agents as single geometric points.  It is the correct choice because an agent is a sphere: its center position is the only geometric data needed.  Radius/diameter are stored as point attributes, not as geometry, so that ParaView's `Glyph` filter can scale spherical glyphs by the diameter attribute.

`VTK_VOXEL` (type 11) is available in the standalone adaptor as an alternative diffusion output.  It is the axis-aligned 3D cell type that `vtkUnstructuredGridVolumeMapper` supports.  The default VTI path avoids this cell type entirely — `vtkSmartVolumeMapper` works directly on ImageData PointData.

---

## 15. Glossary

| Term | Meaning |
|------|---------|
| **VTK** | Visualization Toolkit — the C++ library underlying ParaView |
| **ParaView** | Open-source scientific visualization application built on VTK |
| **Catalyst** | VTK/ParaView in-situ co-processing library (`vtkCPProcessor`) |
| **VTU** | VTK XML Unstructured Grid — file format for arbitrary-topology meshes |
| **PVTU** | Parallel VTU — lightweight XML index listing piece `.vtu` files |
| **VTI** | VTK XML Image Data — file format for regular structured (uniform) grids |
| **PVTI** | Parallel VTI — lightweight XML index listing piece `.vti` files |
| **Appended binary** | VTK data encoding: offsets in XML, raw bytes after `</UnstructuredGrid>` |
| **PointData** | VTK data attribute: one value per mesh point (vertex) |
| **CellData** | VTK data attribute: one value per mesh cell (voxel, triangle, …) |
| **VTK_VOXEL** | VTK cell type 11 — axis-aligned hexahedron with specific vertex ordering |
| **VTK_VERTEX** | VTK cell type 1 — 0D degenerate cell representing a single point |
| **TPluginManager** | ROOT mechanism for loading shared libraries and calling factory functions at runtime |
| **MappedDataArray** | BioDynaMo class that creates a zero-copy VTK view into agent memory |
| **bdm.rootrc** | ROOT configuration file read at startup; registers `VisualizationAdaptor` plugins |
| **TDataMember** | ROOT reflection class that provides byte-offset access to C++ struct fields |
| **Z-slab** | A horizontal slice of the diffusion grid covering all X and Y but a subset of Z boxes — one per OpenMP thread |
