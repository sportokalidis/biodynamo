# Radiation Necrosis Simulation

This simulation models radiation necrosis after radiation therapy treatment of brain metastases, based on computational approaches described in the research literature.

## Overview

The simulation models:

1. **Healthy Brain Tissue**: Neurons and glial cells that make up normal brain tissue
2. **Brain Metastases**: Multiple small tumor lesions that have spread to the brain
3. **Radiation Therapy**: Stereotactic radiosurgery (SRS) treatment targeting the metastases
4. **Radiation Effects**: Immediate and delayed cellular damage from radiation
5. **Radiation Necrosis**: Progressive tissue death that can occur months after radiation
6. **Inflammatory Response**: Secondary inflammation that contributes to necrosis
7. **Diffusion of Factors**: Toxic factors, oxygen, and nutrients that influence cell survival

## Key Features

### Cell Types
- **Healthy Neurons** (70% of healthy tissue): Moderately resistant to radiation
- **Healthy Glia** (30% of healthy tissue): Standard radiation sensitivity  
- **Tumor Cells**: More sensitive to radiation, can grow and divide
- **Necrotic Cells**: Dead cells that secrete toxic inflammatory factors
- **Inflammatory Cells**: Recruited in response to tissue damage

### Radiation Therapy Model
- **Treatment Protocol**: Single-fraction stereotactic radiosurgery
- **Dose**: 30 Gy (typical for brain metastases)
- **Timing**: Delivered at simulation step 100
- **Targeting**: Large field covering multiple metastases
- **Dose Distribution**: Decreases with distance from treatment center

### Biological Behaviors
- **Radiation Therapy**: Applies dose-dependent cellular damage
- **Cell Death**: Immediate and delayed cell death based on damage level
- **Toxic Secretion**: Necrotic cells release inflammatory factors
- **Tumor Growth**: Reduced growth rate after radiation damage
- **Inflammatory Response**: Recruitment of inflammatory cells to damaged areas

### Diffusion Model
- **Toxic Factors**: Secreted by necrotic and inflammatory cells, cause further damage
- **Oxygen**: Essential for cell survival, uniform distribution with Dirichlet boundaries  
- **Nutrients**: Required for cell metabolism and repair

## Simulation Timeline

- **Steps 0-99**: Initial tumor growth phase
- **Step 100**: Radiation therapy delivery
- **Steps 101-500**: Post-radiation effects and progressive necrosis development

## Expected Outcomes

The simulation demonstrates:

1. **Immediate Effects**: Rapid tumor cell death and some normal tissue damage
2. **Delayed Necrosis**: Progressive tissue death developing over time
3. **Inflammatory Cascade**: Recruitment of inflammatory cells amplifying damage
4. **Spatial Patterns**: Necrosis typically develops near the radiation field edges
5. **Toxic Factor Spread**: Diffusion of inflammatory factors affecting nearby healthy tissue

## Building and Running

```bash
# Navigate to the radiation necrosis demo directory
cd biodynamo/demo/radiation_necrosis

# Build the simulation
bdm build

# Run the simulation  
bdm run

# Run with visualization output
bdm run --visualize
```

## Visualization

The simulation outputs visualization data that can be analyzed with ParaView:

- **Cell Type Coloring**: Different colors for healthy, tumor, necrotic, and inflammatory cells
- **Damage Level**: Visualization of cellular damage levels (0-1.0)
- **Cumulative Dose**: Total radiation dose received by each cell
- **Viability Status**: Whether cells remain viable after treatment

## Clinical Relevance

This computational model helps understand:

- **Risk Factors**: Conditions that increase radiation necrosis risk
- **Dose Optimization**: Balancing tumor control with normal tissue toxicity
- **Timing Patterns**: When necrosis is most likely to develop
- **Treatment Planning**: Optimizing radiation fields to minimize healthy tissue exposure

## References

Based on concepts from computational modeling of radiation therapy effects and the development of radiation necrosis in brain tissue following stereotactic radiosurgery for brain metastases.
