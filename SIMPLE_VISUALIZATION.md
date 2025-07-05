# Simple VTK-Independent Visualization Export for BioDynaMo

This implementation adds VTK-independent visualization export capability to BioDynaMo, allowing VTU and VTI file export without requiring ParaView/VTK libraries.

## Overview

The simple visualization system provides an alternative to the ParaView-based visualization export when ParaView is not available or when a lightweight export solution is needed.

## Features

- **VTU file export** for agents (without VTK dependencies)
- **VTI file export** for diffusion grids (without VTK dependencies)
- **Parallel export support** with PVTU/PVTI files
- **Automatic fallback** when ParaView is not available
- **Thread-safe** implementation for multi-threaded simulations
- **Compatible** with existing BioDynaMo parameter configurations

## Architecture

### Core Components

1. **SimpleVtuWriter**: Writes VTU files using plain C++ file I/O
2. **SimpleVtiWriter**: Writes VTI files for diffusion grids
3. **SimpleVisualizationAdaptor**: Coordinates the export process
4. **SimpleAdaptor**: Integrates with BioDynaMo's VisualizationAdaptor interface

### File Structure

```
src/core/visualization/simple/
├── simple_vtu_writer.h           # VTU writer header
├── simple_vtu_writer.cc          # VTU writer implementation
├── simple_vti_writer.h           # VTI writer header
├── simple_vti_writer.cc          # VTI writer implementation
├── simple_visualization_adaptor.h # Main adaptor header
├── simple_visualization_adaptor.cc # Main adaptor implementation
├── simple_visualization_op.h     # Operation header
└── simple_visualization_op.cc    # Operation implementation

src/core/visualization/
├── simple_adaptor.h              # Interface adaptor header
└── simple_adaptor.cc             # Interface adaptor implementation
```

## Configuration

### CMake Options

- `simple_visualization`: Enable simple VTK-independent visualization export (default: ON)
- `paraview`: Enable ParaView visualization (default: ON)

### Build Configurations

1. **ParaView + Simple Visualization** (default):
   ```bash
   cmake .. -Dparaview=ON -Dsimple_visualization=ON
   ```

2. **Simple Visualization Only** (no ParaView dependency):
   ```bash
   cmake .. -Dparaview=OFF -Dsimple_visualization=ON
   ```

3. **ParaView Only** (legacy mode):
   ```bash
   cmake .. -Dparaview=ON -Dsimple_visualization=OFF
   ```

### Runtime Configuration

The visualization engine is automatically selected based on availability:

1. If ParaView is available: uses "paraview" engine
2. If ParaView is not available but simple visualization is enabled: uses "simple" engine
3. If neither is available: no visualization export

You can also explicitly set the engine in your simulation parameters:
```cpp
param->visualization_engine = "simple";
```

## Usage

### Basic Usage

The simple visualization system integrates seamlessly with existing BioDynaMo code:

```cpp
#include "biodynamo.h"

using namespace bdm;

int main() {
    Simulation simulation("my_simulation");
    auto* param = simulation.GetParam();
    
    // Enable visualization export (same as before)
    param->export_visualization = true;
    param->visualization_interval = 10;
    
    // Specify agents to visualize
    param->visualize_agents["Cell"] = {};
    
    // Specify diffusion grids to visualize
    param->visualize_diffusion.push_back({"substance", true, true});
    
    // Run simulation - visualization will automatically use 
    // simple export if ParaView is not available
    simulation.GetScheduler()->Simulate(1000);
    
    return 0;
}
```

### Output Files

The simple visualization system generates the same file structure as ParaView:

```
output/
├── simulation_info.json          # Simulation metadata
├── Cell-0.vtu                    # Agent data (step 0)
├── Cell-10.vtu                   # Agent data (step 10)
├── Cell-0.pvtu                   # Parallel VTU (if multi-threaded)
├── substance-0.vti               # Diffusion grid data
└── substance-10.vti              # Diffusion grid data
```

### Multi-threading Support

When running with multiple threads, the system automatically creates parallel files:

- Individual VTU files per thread: `AgentType-step_threadId.vtu`
- Parallel VTU file: `AgentType-step.pvtu` (references individual files)

## Implementation Details

### VTU File Format

The simple VTU writer generates standard VTK XML format files with:

- **Points**: Agent positions (x, y, z coordinates)
- **Point Data**: Agent attributes (ID, diameter, volume, mass, etc.)
- **Cells**: Connectivity information (each agent is a vertex cell)

### VTI File Format

The simple VTI writer generates standard VTK XML format files with:

- **Structured Grid**: Regular grid for diffusion data
- **Point Data**: Concentration and gradient values
- **Extent**: Grid dimensions and spacing

### Thread Safety

The implementation is thread-safe through:

- **Thread-local file writers**: Each thread writes to separate files
- **Atomic operations**: For shared data structures
- **OpenMP synchronization**: For parallel regions

## Compatibility

### File Format Compatibility

Files generated by the simple visualization system are fully compatible with:

- **ParaView**: Can open and visualize the VTU/VTI files
- **VisIt**: Alternative visualization tool
- **Python VTK**: For custom processing scripts
- **Other VTK-based tools**: Standard format compliance

### BioDynaMo Integration

The simple visualization system:

- **Respects all existing parameters**: visualization_interval, visualize_agents, etc.
- **Maintains the same API**: No changes needed to existing simulation code
- **Provides automatic fallback**: Seamlessly switches when ParaView is unavailable
- **Supports all agent types**: Works with custom agent implementations

## Performance

### Memory Usage

The simple visualization system has lower memory overhead compared to ParaView:

- **No VTK data structures**: Direct file writing without intermediate objects
- **Streaming writes**: Data written directly to disk
- **Minimal buffering**: Only what's needed for file I/O

### Performance Characteristics

- **Faster startup**: No VTK library initialization
- **Lower memory footprint**: No VTK object overhead
- **Comparable I/O speed**: Direct file writing is efficient
- **Smaller binary size**: No VTK/ParaView library dependencies

## Limitations

### Current Limitations

1. **Agent visualization**: Currently supports basic cell attributes (position, diameter, volume, mass)
2. **File format**: ASCII format only (binary format not implemented)
3. **Compression**: No compression support (unlike ParaView)
4. **Advanced features**: No support for complex VTK features (advanced cell types, time-varying data)

### Future Enhancements

Potential improvements for future versions:

1. **Binary format support**: For better performance and smaller files
2. **Compression**: Reduce file sizes
3. **Custom attributes**: Support for user-defined agent properties
4. **Advanced cell types**: Support for more complex geometries
5. **Time series**: PVD files for time series visualization

## Testing

### Unit Tests

The implementation includes comprehensive tests:

```bash
# Run visualization tests
make test ARGS="-R visualization"
```

### Integration Tests

Test the complete export pipeline:

```bash
# Build and run demo with simple visualization
cmake .. -Dparaview=OFF -Dsimple_visualization=ON
make
cd demo/cell_division
make
./cell_division
# Check output files in cell_division/output/
```

## Troubleshooting

### Common Issues

1. **Files not generated**: Check `param->export_visualization = true`
2. **Empty files**: Ensure agents/grids exist during export steps
3. **Permission errors**: Check write permissions in output directory
4. **Large files**: Consider increasing `visualization_interval`

### Debug Mode

Enable detailed logging:

```cpp
param->debug = true;  // Enable debug output
```

### File Validation

Verify generated files:

```bash
# Check VTU file structure
xmllint --format output/Cell-0.vtu | head -20

# Check file sizes
ls -la output/*.vtu output/*.vti
```

## Examples

### Complete Example

See `demo/cell_division` with simple visualization:

```cpp
#include "biodynamo.h"

using namespace bdm;

int main() {
    Simulation simulation("cell_division");
    auto* param = simulation.GetParam();
    
    // Configure simple visualization
    param->export_visualization = true;
    param->visualization_interval = 10;
    param->visualization_engine = "simple";  // Force simple mode
    
    // Set up agents to visualize
    param->visualize_agents["Cell"] = {};
    
    // Create initial cell
    auto* rm = simulation.GetResourceManager();
    auto* cell = new Cell({0, 0, 0});
    cell->SetDiameter(10.0);
    rm->AddAgent(cell);
    
    // Run simulation
    simulation.GetScheduler()->Simulate(100);
    
    return 0;
}
```

This implementation provides a robust, lightweight alternative to ParaView-based visualization while maintaining full compatibility with BioDynaMo's existing visualization infrastructure.
