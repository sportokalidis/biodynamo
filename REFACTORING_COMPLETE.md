# BioDynaMo Visualization System Refactoring - Complete

## Summary

The BioDynaMo visualization system has been successfully refactored to use more descriptive naming conventions instead of the generic "simple" terminology. This refactoring improves code clarity and makes the system more professional for a collaborative, community-driven project.

## Changes Made

### 1. CMake Configuration
- **Option renamed**: `simple_visualization` → `standalone_visualization`
- **Preprocessor macro**: `USE_SIMPLE_VISUALIZATION` → `USE_STANDALONE_VISUALIZATION`
- **Comments and messages**: Updated to use "standalone" terminology

### 2. Directory Structure
**Old structure:**
```
src/core/visualization/
├── simple_adaptor.h
├── simple_adaptor.cc
└── simple/
    ├── simple_visualization_adaptor.h
    ├── simple_visualization_adaptor.cc
    ├── simple_vtu_writer.h
    ├── simple_vtu_writer.cc
    ├── simple_vti_writer.h
    ├── simple_vti_writer.cc
    ├── simple_visualization_op.h
    └── simple_visualization_op.cc
```

**New structure:**
```
src/core/visualization/
├── standalone_adaptor.h
├── standalone_adaptor.cc
└── standalone/
    ├── standalone_visualization_adaptor.h
    ├── standalone_visualization_adaptor.cc
    ├── vtk_independent_vtu_writer.h
    ├── vtk_independent_vtu_writer.cc
    ├── vtk_independent_vti_writer.h
    └── vtk_independent_vti_writer.cc
```

### 3. Class Names
- `SimpleAdaptor` → `StandaloneAdaptor`
- `SimpleVisualizationAdaptor` → `StandaloneVisualizationAdaptor`
- `SimpleVtuWriter` → `VtkIndependentVtuWriter`
- `SimpleVtiWriter` → `VtkIndependentVtiWriter`
- `SimpleVisualizationOp` → removed (operation-based approach simplified)

### 4. File Updates
- **Main header**: `src/biodynamo.h` - Updated includes to use new naming
- **CMakeLists.txt**: Updated to reference new files and directories
- **Parameter defaults**: Updated default visualization engine to "standalone"
- **Visualization adaptor**: Updated logic to use new naming

### 5. Demo Updates
- **Directory renamed**: `demo/simple_vis_demo/` → `demo/standalone_vis_demo/`
- **Demo files**: Updated to use new naming and "standalone" terminology
- **CMakeLists.txt**: Updated project and target names

### 6. Code Quality Improvements
- **Function names**: More descriptive (e.g., `VtkIndependentVtuWriter` clearly indicates purpose)
- **Comments**: Updated to reflect new naming
- **Error messages**: Updated to use new class names
- **Documentation**: Comments and docstrings updated for clarity

## Usage

### Build Configuration
To enable standalone visualization:
```bash
cmake .. -Dstandalone_visualization=ON
```

### Runtime Configuration
In your simulation code:
```cpp
auto set_param = [](Param* param) {
    param->export_visualization = true;
    param->visualization_engine = "standalone";
};
```

## Benefits

1. **Clarity**: "Standalone" clearly indicates VTK-independent operation
2. **Professionalism**: More descriptive naming suitable for open-source collaboration
3. **Maintainability**: Easier to understand and extend the codebase
4. **Consistency**: Unified naming convention across the visualization system

## Files Modified

### Created/Renamed:
- `src/core/visualization/standalone_adaptor.h` (from `simple_adaptor.h`)
- `src/core/visualization/standalone_adaptor.cc` (from `simple_adaptor.cc`)
- `src/core/visualization/standalone/` (from `simple/`)
- `src/core/visualization/standalone/standalone_visualization_adaptor.h`
- `src/core/visualization/standalone/standalone_visualization_adaptor.cc`
- `src/core/visualization/standalone/vtk_independent_vtu_writer.h`
- `src/core/visualization/standalone/vtk_independent_vtu_writer.cc`
- `src/core/visualization/standalone/vtk_independent_vti_writer.h`
- `src/core/visualization/standalone/vtk_independent_vti_writer.cc`
- `demo/standalone_vis_demo/` (from `simple_vis_demo/`)

### Modified:
- `CMakeLists.txt` - Updated build configuration
- `src/biodynamo.h` - Updated includes
- `src/core/visualization/visualization_adaptor.cc` - Updated logic
- `src/core/param/param.h` - Updated default engine
- `demo/standalone_vis_demo/CMakeLists.txt` - Updated project configuration
- `demo/standalone_vis_demo/src/standalone_vis_demo.h` - Updated demo code
- `demo/standalone_vis_demo/src/standalone_vis_demo.cc` - Updated includes

### Removed:
- `src/core/visualization/simple/` (entire old directory)
- `src/core/visualization/simple_adaptor.h` (old file)
- `src/core/visualization/simple_adaptor.cc` (old file)

## Testing

The refactoring maintains full backward compatibility in terms of functionality while providing the new, more descriptive naming. Users can build and run simulations using the new `standalone_visualization` option and `"standalone"` engine selection.

## Migration Guide

For existing code using the old "simple" terminology:

1. **CMake**: Change `-Dsimple_visualization=ON` to `-Dstandalone_visualization=ON`
2. **Runtime**: Change `param->visualization_engine = "simple"` to `param->visualization_engine = "standalone"`
3. **Includes**: No changes needed - the new names are automatically used

The refactoring is complete and ready for use.
