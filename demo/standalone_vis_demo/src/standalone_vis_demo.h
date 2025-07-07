// Standalone BioDynaMo simulation with VTK-independent visualization
#include "biodynamo.h"

using namespace bdm;

// Simple cell that grows and divides
class MyCell : public Cell {
  BDM_AGENT_HEADER(MyCell, Cell, 1);

 public:
  MyCell() {}
  explicit MyCell(const Real3& position) : Base(position) {}
  virtual ~MyCell() {}

  /// If MyCell divides, the daughter has to initialize its attributes
  void Initialize(const NewAgentEvent& event) override {
    Base::Initialize(event);
    // Add any initialization logic here if needed
  }

  // Make the cell grow
  void RunDiscretization() override {
    if (GetDiameter() < 20) {
      ChangeVolume(1.1);  // Grow by 10% each step
    } else {
      Divide();  // Divide when diameter reaches 20
    }
  }
};

inline int Simulate(int argc, const char** argv) {
  // Configure simulation parameters
  auto set_param = [](Param* param) {
    // Configure standalone visualization
    param->export_visualization = true;
    param->visualization_interval = 10;  // Export every 10 steps
    
    // Use standalone visualization engine (automatic fallback if ParaView unavailable)
    param->visualization_engine = "standalone";
    
    // Configure what to visualize
    param->visualize_agents["MyCell"] = {};  // Visualize MyCell agents
  };
  
  // Create simulation
  Simulation simulation(argc, argv, set_param);
  
  // Create some initial cells
  auto* rm = simulation.GetResourceManager();
  auto* random = simulation.GetRandom();
  
  // Create 10 initial cells at random positions
  for (int i = 0; i < 10; i++) {
    Real3 position = {random->Uniform(-50, 50), 
                      random->Uniform(-50, 50), 
                      random->Uniform(-50, 50)};
    auto* cell = new MyCell(position);
    cell->SetDiameter(10.0);
    rm->AddAgent(cell);
  }
  
  std::cout << "Starting simulation with standalone visualization..." << std::endl;
  std::cout << "Output files will be saved to: " << simulation.GetOutputDir() << std::endl;
  
  // Run simulation for 100 steps
  simulation.GetScheduler()->Simulate(100);
  
  std::cout << "Check the output directory for VTU files." << std::endl;
  std::cout << "Simulation completed successfully!\n";

  return 0;
}
