/*
BSD 3-Clause License

Copyright (c) 2020, The Regents of the University of Minnesota

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#pragma once

#include "gmat.h"
#include "odb/db.h"
#include "utl/Logger.h"

namespace sta {
class dbSta;
class Corner;
}  // namespace sta

namespace rsz {
class Resizer;
}

namespace psm {

//! Class for IR solver
/*
 * Builds the equations GV=J and uses SuperLU
 * to solve the matrix equations
 */
struct ViaCut
{
  Point loc = Point(0, 0);
  NodeEnclosure bot_encl{0, 0, 0, 0};
  NodeEnclosure top_encl{0, 0, 0, 0};
};

class IRSolver
{
 public:
  struct SourceData
  {
    int x;
    int y;
    int size;
    double voltage;
    int layer;
    bool user_specified;
  };

  //! Constructor for IRSolver class
  /*
   * This constructor creates an instance of the class using
   * the given inputs.
   */
  IRSolver(odb::dbDatabase* db,
           sta::dbSta* sta,
           rsz::Resizer* resizer,
           utl::Logger* logger,
           const std::string& vsrc_loc,
           const std::string& power_net,
           const std::string& out_file,
           const std::string& error_file,
           const std::string& em_out_file,
           const std::string& spice_out_file,
           bool em_analyze,
           int bump_pitch_x,
           int bump_pitch_y,
           float node_density_um,
           int node_density_factor_user,
           const std::map<std::string, float>& net_voltage_map,
           sta::Corner* corner);
  //! IRSolver destructor
  ~IRSolver();
  //! Returns the created G matrix for the design
  GMat* getGMat();
  //! Returns current map represented as a 1D vector
  std::vector<double> getJ();
  //! Function to solve for IR drop
  void solveIR();
  //! Function to get the power value from OpenSTA
  std::vector<std::pair<odb::dbInst*, double>> getPower();
  std::pair<double, double> getSupplyVoltage();

  bool getConnectionTest();

  int getMinimumResolution();

  int printSpice();

  bool build();
  bool buildConnection();

  const std::vector<SourceData>& getSources() const { return sources_; }

  double getWorstCaseVoltage() const { return wc_voltage; }
  double getMaxCurrent() const { return max_cur; }
  double getAvgCurrent() const { return avg_cur; }
  int getNumResistors() const { return num_res; }
  double getAvgVoltage() const { return avg_voltage; }
  float getSupplyVoltageSrc() const { return supply_voltage_src; }

 private:
  //! Function to add sources to the G matrix
  bool addSources();
  //! Function that parses the Vsrc file
  void readSourceData(bool require_voltage);
  bool createSourcesFromBTerms(odb::dbNet* net, double voltage);
  bool createSourcesFromPads(odb::dbNet* net, double voltage);
  //! Function to create a J vector from the current map
  bool createJ();
  //! Function to create a G matrix using the nodes
  bool createGmat(bool connection_only = false);
  //! Function to find and store the upper and lower PDN layers and return a
  //! list
  // of wires for all PDN tasks
  void findPdnWires(odb::dbNet* power_net);
  //! Function to create the nodes of vias in the G matrix
  void createGmatViaNodes();
  //! Function to create the nodes of wires in the G matrix
  void createGmatWireNodes(const std::vector<odb::Rect>& macros);
  //! Function to find and store the macro boundaries
  std::vector<odb::Rect> getMacroBoundaries();

  NodeEnclosure getViaEnclosure(int layer, odb::dbSet<odb::dbBox> via_boxes);

  std::map<Point, ViaCut> getViaCuts(Point loc,
                                     odb::dbSet<odb::dbBox> via_boxes,
                                     int lb,
                                     int lt,
                                     bool has_params,
                                     const odb::dbViaParams& params);

  //! Function to create the nodes for the sources
  int createSourceNodes(bool connection_only, int unit_micron);
  //! Function to create the connections of the G matrix
  void createGmatConnections(bool connection_only);
  bool checkConnectivity(bool connection_only = false);
  bool checkValidR(double R);
  bool getResult();

  double getResistance(odb::dbTechLayer* layer) const;

  float supply_voltage_src{0};
  //! Worst case voltage at the lowest layer nodes
  double wc_voltage{0};
  //! Worst case current at the lowest layer nodes
  double max_cur{0};
  //! Average current at the lowest layer nodes
  double avg_cur{0};
  //! number of resistances
  int num_res{0};
  //! Average voltage at lowest layer nodes
  double avg_voltage{0};
  //! Vector of worstcase voltages in the lowest layers
  std::vector<double> wc_volt_layer;
  //! Pointer to the Db
  odb::dbDatabase* db_;
  //! Pointer to STA
  sta::dbSta* sta_;
  //! Pointer to Resizer for parastics
  rsz::Resizer* resizer_;
  //! Pointer to Logger
  utl::Logger* logger_;
  //! Voltage source file
  std::string vsrc_file_;
  std::string power_net_;
  //! Resistance configuration file
  std::string out_file_;
  std::string error_file_;
  std::string em_out_file_;
  bool em_flag_;
  std::string spice_out_file_;
  //! G matrix for voltage
  std::unique_ptr<GMat> Gmat_;
  //! Node density in the lower most layer to append the current sources
  int node_density_{0};              // Initialize to zero
  int node_density_factor_{5};       // Default value
  int node_density_factor_user_{0};  // User defined value
  float node_density_um_{-1};  // Initialize to negative unless set by user
  //! Routing Level of the top layer
  int top_layer_{0};
  int bump_pitch_x_{0};
  int bump_pitch_y_{0};
  int bump_pitch_default_{140};
  int bump_size_{10};

  int bottom_layer_{10};

  bool result_{false};
  bool connection_{false};

  sta::Corner* corner_;

  odb::dbSigType power_net_type_;
  std::map<std::string, float> net_voltage_map_;
  //! Current vector 1D
  std::vector<double> J_;
  //! source locations and values
  std::vector<SourceData> sources_;
  //! Per unit R and via R for each routing layer
  std::vector<std::tuple<int, double, double>> layer_res_;
  //! Locations of the source in the G matrix
  std::map<NodeIdx, double> source_nodes_;

  std::vector<odb::dbSBox*> power_wires_;
};
}  // namespace psm
