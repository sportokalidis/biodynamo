# Visualization Benchmark: Standalone vs ParaView

Benchmark using the **soma_clustering** demo on the `paraview-exporter-opt` branch.

**Setup:**
- Demo: `soma_clustering` (200 simulation steps, export every 10 steps → 100 exported snapshots)
- Diffusion: `Substance_0`, concentration + gradient
- Threads: 14 OpenMP threads (same for both runs)
- Machine: Linux 6.14, x86_64
- Build type: Release

---

## 1. Build configuration

| | Standalone | ParaView |
|---|---|---|
| CMake flags | `-Dparaview=OFF -Dstandalone_visualization=ON` | `-Dparaview=ON -Dstandalone_visualization=OFF` |
| Build directory | `build/` | `build-paraview/` |
| Shared library produced | `libVisualizationAdaptor.so` | `libVisualizationAdaptor.so` |
| VTK/ParaView dependency | None | ~500 MB ParaView install |
| `bdm.toml` adaptor key | `adaptor = "standalone"` | `adaptor = "paraview"` (or omit — default) |

---

## 2. Simulation timing

| Metric | Standalone | ParaView |
|---|---|---|
| **Wall clock time** | **13.82 s** | **24.12 s** |
| User time (total CPU) | 96.78 s | 112.79 s |
| CPU utilisation | 722% (~7 threads active) | 486% (~5 threads active) |
| Peak RSS memory | 450 MB | 743 MB |

**Standalone is ~1.7× faster** wall-clock and uses ~40% less peak memory.

The higher CPU utilisation in the standalone run reflects more effective OpenMP
parallelism: the standalone exporter spawns one thread per piece for both
agents and diffusion, with no synchronisation overhead from VTK's internal
pipeline. The ParaView path allocates VTK in-memory objects (`vtkUnstructuredGrid`,
`vtkImageData`) and then serialises through the VTK writer classes, which adds
per-step heap allocation and lock contention.

The larger RSS in the ParaView run comes from loading the full ParaView/VTK
shared libraries plus allocating live VTK data structures alongside the
simulation objects.

---

## 3. Output file counts

| Category | Standalone | ParaView |
|---|---|---|
| Agent piece files | 1 400 `.vtu` | 1 400 `.vtu` |
| Agent index files | 100 `.pvtu` | 100 `.pvtu` |
| Diffusion piece files | 1 400 `.vti` | 1 300 `.vti` |
| Diffusion index files | 100 `.pvti` | 100 `.pvti` |
| Extra metadata files | — | 2 (`.json` + `.pvsm`) |
| **Total files** | **3 000** | **2 902** |

Both runs produce 14 pieces per snapshot (one per OpenMP thread).

The ParaView run produces one fewer diffusion piece per step (1 300 vs 1 400)
because VTK's `ParallelVtiWriter` may merge thin Z-slabs when the grid
size doesn't divide evenly across threads.

ParaView also writes `simulation_info.json` and a `.pvsm` ParaView state file
(enabled by `visualization_export_generate_pvsm = true` in `Param`) that allows
the user to reload the full scene in ParaView with a single click. Standalone
does not generate these.

---

## 4. Total output size

| Category | Standalone | ParaView | Ratio |
|---|---|---|---|
| Agent pieces | 200 MB | 94 MB | 2.1× larger |
| Diffusion pieces | 196 MB | 250 MB | 1.3× **smaller** |
| **Total** | **396 MB** | **345 MB** | **1.1× larger** |

The standalone output is within 15% of ParaView's footprint. The remaining
gap is entirely due to agents:

1. **No compression.** Standalone writes raw binary (no zlib). ParaView uses
   `vtkZLibDataCompressor` on every array. Agent positions and scalar fields
   compress well; the 2× size difference on agent files is mainly due to this.
2. **Float64 for all agent scalars.** Standalone writes every scalar as
   `Float64` (8 bytes). ParaView writes `cell_type_` as `Int32` (4 bytes) and
   honours the native type of each field.

Diffusion files are actually **smaller** in standalone because BioDynaMo's
diffusion concentrations at non-zero steps do not compress as efficiently as
the all-zero step-0 data that dominates ParaView's total. Both implementations
now use VTI ImageData for diffusion.

---

## 5. Agent VTU file: side-by-side (step 0, piece 0)

### Standalone — `agents_0_p0.vtu`

```
File size:  149 783 bytes
Agents:     2 286
```

```xml
<VTKFile type="UnstructuredGrid" version="0.1"
         byte_order="LittleEndian" header_type="UInt32">
  <UnstructuredGrid>
    <Piece NumberOfPoints="2286" NumberOfCells="2286">
      <PointData>
        <DataArray type="UInt64"   Name="Cell_ID"    format="appended" offset="0"/>
        <DataArray type="Float64"  Name="Diameter"   format="appended" offset="18292"/>
        <DataArray type="Float64"  Name="diameter_"  format="appended" offset="36584"/>
        <DataArray type="Float64"  Name="cell_type_" format="appended" offset="54876"/>
      </PointData>
      <Points>
        <DataArray type="Float64" NumberOfComponents="3" format="appended" offset="73168"/>
      </Points>
      <Cells>
        <DataArray Name="connectivity" type="Int32" format="appended" offset="128036"/>
        <DataArray Name="offsets"      type="Int32" format="appended" offset="137184"/>
        <DataArray Name="types"        type="UInt8" format="appended" offset="146332"/>
      </Cells>
    </Piece>
  </UnstructuredGrid>
  <AppendedData encoding="raw">
    _[raw binary: each array preceded by 4-byte uint32 length]
  </AppendedData>
</VTKFile>
```

**Key characteristics:**
- Encoding: raw appended binary — no compression, no Base64
- All scalars: `Float64` (8 bytes/value); Cell_ID: `UInt64`
- Topology: explicit `VTK_VERTEX` (type 1) — `NumberOfCells == NumberOfPoints`
- Fields: user-requested (`diameter_`, `cell_type_`) plus standard BioDynaMo
  fields (`Cell_ID`, `Diameter`)

### ParaView — `MyCell-0_0.vtu`

```
File size:  69 121 bytes
Agents:     2 286
```

```xml
<VTKFile type="UnstructuredGrid" version="0.1"
         byte_order="LittleEndian" header_type="UInt32"
         compressor="vtkZLibDataCompressor">
  <UnstructuredGrid>
    <Piece NumberOfPoints="2286" NumberOfCells="0">
      <PointData>
        <DataArray type="Int32"   Name="cell_type_" format="binary" RangeMin="-1" RangeMax="1">
          [Base64-encoded zlib-compressed data]
        </DataArray>
        <DataArray type="Float64" Name="diameter_" format="binary" RangeMin="10" RangeMax="10">
          [Base64-encoded zlib-compressed data]
        </DataArray>
      </PointData>
      <Points>
        <DataArray type="Float64" Name="position_" NumberOfComponents="3" format="binary">
          [Base64-encoded zlib-compressed data]
        </DataArray>
      </Points>
      <Cells>
        <!-- connectivity/offsets/types (all empty — NumberOfCells="0") -->
      </Cells>
    </Piece>
  </UnstructuredGrid>
</VTKFile>
```

**Key characteristics:**
- Encoding: `format="binary"` (Base64) + zlib compression per array block
- `NumberOfCells="0"` — ParaView's VTK writer omits the Cells section when all
  cells are degenerate VTK_VERTEX, reducing file size
- Fields: only `cell_type_` (as `Int32`) and `diameter_` (as `Float64`) —
  only what is listed under `additional_data_members` in `bdm.toml`
- `RangeMin`/`RangeMax` attributes: written by VTK automatically; allow
  ParaView to set colour-map ranges without decoding the compressed data

---

## 6. Diffusion file: side-by-side (step 0, piece 0)

### Standalone — `diffusion_Substance_0_0_p0.vti`

```
File size:  154 193 bytes   (VTI — Image Data)
Format:     ImageData, CellData
WholeExtent: 0 40 0 40 0 40
Piece Extent: 0 40 0 40 0 3  (first 3 Z-slabs, 4 800 cells)
Spacing:     20 20 20
Origin:      0 0 0
```

```xml
<VTKFile type="ImageData" version="0.1"
         byte_order="LittleEndian" header_type="UInt32">
  <ImageData WholeExtent="0 40 0 40 0 40"
             Origin="0 0 0" Spacing="20 20 20">
    <Piece Extent="0 40 0 40 0 3">
      <CellData>
        <DataArray type="Float64" Name="Substance Concentration"
                   NumberOfComponents="1" format="appended" offset="0"/>
        <DataArray type="Float64" Name="Diffusion Gradient"
                   NumberOfComponents="3" format="appended" offset="38404"/>
      </CellData>
    </Piece>
  </ImageData>
  <AppendedData encoding="raw">
    _[raw binary]
  </AppendedData>
</VTKFile>
```

**Key characteristics:**
- VTK ImageData format — no explicit geometry arrays; the full grid is
  described by `Origin`, `Spacing`, and `Extent` alone
- `CellData` — one value per diffusion box; `WholeExtent` uses the VTK
  N+1-node convention (41 nodes for 40 cells per dimension)
- Adjacent pieces share their boundary node (`0 3`, `3 6`, …, `39 40`) so
  `vtkXMLPImageDataReader` assembles all pieces without gaps
- Raw appended binary, no compression

### ParaView — `Substance_0-0_0.vti`

```
File size:  1 288 bytes     (VTI — Image Data)
Format:     ImageData, PointData
WholeExtent: 0 39 0 39 0 39
Spacing:     20 20 20
Origin:      10 10 10
```

Both implementations use VTK ImageData — no explicit geometry arrays. At step 0
all diffusion values are zero, so ParaView's zlib-compressed data arrays shrink
to a handful of bytes. Standalone raw binary stays at ~150 KB regardless of
concentration values.

**Size ratio at step 0:** 154 193 / 1 288 = **120×** larger for standalone.
At later steps ParaView VTI files grow to ~20–30 KB; standalone remains ~150 KB.
The overall diffusion totals (196 MB vs 250 MB) flip because at non-zero steps
the actual concentration data does not compress as efficiently as all-zeros.

---

## 7. File naming conventions

| | Standalone | ParaView |
|---|---|---|
| Agent piece | `agents_{N}_p{p}.vtu` | `{TypeName}-{simstep}_{p}.vtu` |
| Agent index | `agents_{N}.pvtu` | `{TypeName}-{simstep}.pvtu` |
| Diffusion piece | `diffusion_{name}_{N}_p{p}.vti` | `{name}-{simstep}_{p}.vti` |
| Diffusion index | `diffusion_{name}_{N}.pvti` | `{name}-{simstep}.pvti` |
| Output subdirectory | `output/<sim_name>/viz/` | `output/<sim_name>/` |

**N** in standalone is an internal monotonic counter that starts at 0 and
increments once per exported step (after both agent and diffusion files are
written), so `agents_{N}` and `diffusion_{name}_{N}` always share the same N.

**simstep** in ParaView is the actual simulation step number (0, 10, 20, …
at `interval = 10`), making it easier to correlate files with simulation time.

> **Note:** The standalone step counter (`N`) is not the same as the simulation
> step. To correlate standalone files to simulation time, multiply by
> `visualization_interval` (here, interval = 10, so `agents_3` corresponds to
> simulation step 30).

---

## 8. Summary

| Criterion | Standalone | ParaView |
|---|---|---|
| Wall-clock time | **13.8 s** ✓ | 24.1 s |
| Peak memory | **450 MB** ✓ | 743 MB |
| Total output size | 396 MB | **345 MB** ✓ |
| Agent file size (per piece, step 0) | 150 KB | **68 KB** ✓ |
| Diffusion file size (per piece, step 0) | 150 KB | **1.3 KB** ✓ |
| ParaView dependency | None ✓ | ~500 MB install |
| Compression | No | zlib ✓ |
| Volume rendering support | Yes (VTI CellData) ✓ | Yes (VTI PointData) ✓ |
| Automatic `.pvsm` state file | No | Yes ✓ |
| Fields exported (agents) | User + Cell_ID + Diameter | User-specified only ✓ |
| Diffusion format | VTI + implicit geometry ✓ | VTI + implicit geometry ✓ |

**When to use standalone:** when ParaView/VTK is not available, when simulation
speed is critical, or when building on a minimal system. The 1.7× speed
advantage and zero external dependencies make it the right choice for CI,
cluster nodes without a display, and rapid iteration.

**When to use ParaView:** when agent output file size matters (2× smaller due to
zlib compression), when the `.pvsm` auto-generated state file is useful, or
when the in-situ Catalyst pipeline (live rendering during the simulation) is
needed.

**Main open optimization for standalone:** adding optional zlib compression
(matching VTK's `vtkZLibDataCompressor`) would reduce agent output from ~200 MB
to an estimated ~95 MB, bringing the total below ParaView's footprint while
preserving the speed advantage.
