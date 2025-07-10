# BioDynaMo Visualization System - Improved Naming Conventions

## Overview
This document proposes improved naming conventions for the BioDynaMo visualization system to make the codebase more professional and descriptive for community developers.

## Current Issues with Naming
- Generic term "simple" doesn't describe functionality
- Unclear what "simple" means in different contexts
- Not intuitive for new developers joining the project

## Proposed Naming Improvements

### 1. Build System (CMake)
**Current:**
```cmake
option(simple_visualization "Enable simple VTK-independent visualization export" ON)
add_definitions("-DUSE_SIMPLE_VISUALIZATION")
```

**Improved:**
```cmake
option(standalone_visualization "Enable standalone VTK-independent visualization export" ON)
add_definitions("-DUSE_STANDALONE_VISUALIZATION")
```

### 2. Class Names
**Current:**
- `SimpleAdaptor` 
- `SimpleVisualizationAdaptor`
- `SimpleVtuWriter`
- `SimpleVtiWriter`
- `SimpleVisualizationOp`

**Improved:**
- `StandaloneAdaptor` → Clarifies this works without external dependencies
- `StandaloneVisualizationAdaptor` → More descriptive of functionality
- `VtkIndependentVtuWriter` → Clearly states what it does
- `VtkIndependentVtiWriter` → Clearly states what it does  
- `StandaloneVisualizationOperation` → More professional naming

### 3. Parameter Names
**Current:**
```cpp
std::string visualization_engine = "simple";
```

**Improved:**
```cpp
std::string visualization_engine = "standalone";
```

### 4. File and Directory Structure
**Current:**
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

**Improved:**
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
    ├── vtk_independent_vti_writer.cc
    ├── standalone_visualization_operation.h
    └── standalone_visualization_operation.cc
```

### 5. Preprocessor Definitions
**Current:**
```cpp
#ifdef USE_SIMPLE_VISUALIZATION
```

**Improved:**
```cpp
#ifdef USE_STANDALONE_VISUALIZATION
```

### 6. Documentation and Comments
**Current:**
```cpp
/// Simple visualization adaptor
```

**Improved:**
```cpp
/// Standalone visualization adaptor that exports VTU/VTI files without ParaView or VTK dependencies
/// This adaptor is designed for environments where full ParaView installation is not available
/// but visualization export is still needed for post-processing with external tools.
```

## Benefits of These Changes

### 1. **Clarity for New Developers**
- "Standalone" immediately conveys independence from external libraries
- "VtkIndependent" clearly states what the limitation/feature is
- More descriptive class names help understand architecture

### 2. **Professional Naming**
- Avoids generic terms like "simple" which don't describe functionality
- Uses industry-standard terminology
- More suitable for academic and commercial use

### 3. **Better Documentation**
- Self-documenting code through descriptive names
- Easier to search for specific functionality
- Clearer API for users

### 4. **Maintainability**
- Easier to understand code purpose without deep diving
- Clearer separation between different visualization backends
- More intuitive for code reviews

## Implementation Strategy

### Phase 1: Backward Compatibility
1. Keep existing "simple" names but mark as deprecated
2. Add new "standalone" names as aliases
3. Update documentation to prefer new names

### Phase 2: Migration
1. Update all internal code to use new names
2. Update build system and CMake files
3. Update user-facing documentation and examples

### Phase 3: Cleanup
1. Remove deprecated "simple" names
2. Clean up any remaining references
3. Update version documentation

## User Impact
- **Breaking Change**: Yes, but can be mitigated with deprecation warnings
- **Benefits**: Clearer API, better understanding of functionality
- **Migration**: Simple find-and-replace for user code

## Alternative Naming Suggestions

If "standalone" is not preferred, other options include:
- `lightweight_visualization` - emphasizes minimal dependencies
- `portable_visualization` - emphasizes cross-platform compatibility  
- `native_visualization` - emphasizes built-in functionality
- `embedded_visualization` - emphasizes integration without external deps

## Conclusion
These naming improvements would make BioDynaMo more professional and accessible to the development community while clearly communicating the purpose and capabilities of each component.
