#ifndef OPENSIM_SMITH_2018_ARTICULAR_CONTACT_FORCE_H_
#define OPENSIM_SMITH_2018_ARTICULAR_CONTACT_FORCE_H_
/* -------------------------------------------------------------------------- *
 *                     Smith2018ArticularContactForce.h                      *
 * -------------------------------------------------------------------------- *                                                                        *
 * Author(s): Colin Smith                                                     *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may    *
 * not use this file except in compliance with the License. You may obtain a  *
 * copy of the License at http://www.apache.org/licenses/LICENSE-2.0.         *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 * -------------------------------------------------------------------------- */

// INCLUDE
#include "OpenSim/Simulation/Model/Force.h"
#include "Smith2018ContactMesh.h"
#include "osimPluginDLL.h"


namespace OpenSim {
    /**
* This force class models the articular contact between trianglated surface 
* meshes representing cartilage, mensici, or artfical components. The 
* formulation of the contact model has previously been called an elastic 
* foundation model and discrete element analysis. In this implementation, the 
* non-deforming triangulated meshes are allowed to interpenetrate and the local
* overlap depth is calculated for each triangle. The contact pressure on each
* triangle face is then calculated based on the overlap depth (see below).
* 
* To calculate the local overlap depth, it is necessary to detection the mesh 
* triangles that are in contact. This process is extremely slow if a brute 
* force approach is applied. Smith et al, CMBBE I&V, 2018 introduced a method 
* to efficiently detect contact between triangular meshes using Object Oriented
* Bounding boxes and several additional speed ups that take advantage of the
* constrained nature of articular contact. This approach has been implemented
* in the Smith2018ArticularContactForce component along with some additional 
* features. 
* 
* Two articulating triangular meshes are defined as Smith2018ContactMesh 
* components (Sockets: casting_mesh and target_mesh). The meshes are fixed to
* bodies in the model, and thus their relative poses are determined by the 
* model coordinates. To detect contact, a normal ray is cast from the center of
* each triangle in the casting mesh backwards towards the overlapping target
* mesh. Ray intersection tests are then performed against an Oriented Bounding 
* Box tree constructed around the target mesh. This algorithm is implemented in
* the computeProximity function with the OBB construction and ray intersection
* queries managed by the Smith2018ContactMesh.

* The major speed up in the algorithm leverages the fact that changes in 
* articular contact between time steps are generally small. Thus, after 
* reposing the meshes, (i.e. realizePosition) each triangle in the casting mesh
* is tested against the contacting target triangle in the previous pose. 
* Additional speed up is gained by casting the normal ray in both directions, 
* so even some of the out of contact triangles are "remembered". If the
* previous contacting triangle test fails, the casting ray is checked against 
* the neighboring triangles (those that share a vertex) in the target mesh. 
* Then if this test fails, the expensive casting ray - OBB test is perfromed.
* 
* A linear and non-linear relationship between depth and pressure can be used 
* via the elastic_foundation_formulation property. The implemented equations 
* are those proposed in Bei and Fregly, Med Eng Phys, 2004:
*/
/*
* Linear: 
* \f[
*    P = E*\frac{(1-\nu)}{(1 + \nu)(1-2\nu)\frac{d}{h}}
* \f]
* 
* Non-Linear: 
* \f[
*   P = -E*\frac{(1-\nu)}{(1 + \nu)(1-2\nu)ln(1-\frac{d}{h}})
* \f]
* 
* Where:
* P = pressure
* E = elastic modulus
* \nu = poissons ratio
* d = depth of overlap
* h = height (thickness) of the elastic layer
*
*
* The original Bei and Fregly formulation assumes that a rigid object is 
* contacting an object with a thin elastic layer. This is straightforward to 
* apply to joint replacements where a metal component contacts a polyethelene 
* component. To model cartilage-cartilage contact, this approach requires that
* the two cartilage layers are lumped together, necessitating a constant
* thickness, elastic modulus, and poissons ratio is assumed for the triangles
* in contact. As cartilage-cartilage contact often involves articulations 
* between cartilage surfaces with varying thickness and material properties,
* the Bei and Fregly approach was extended to accomodate variable properties.
* The use_lumped_contact_model property controls whether the constant property
* or variable property formulation is used.
* 
* The variable property formulation is described in Zevenbergen et al, 
* PLOS One, 2018. Here, the following system of four equations must be solved
*  
* \f[
*   P_casting = F(E,\nu,h,d_casting) 
* \f]
* (linear or non-linear formulation above)
*
* \f[
*   P_target = F(E,\nu,h,d_target) 
* \f]
* (linear or non-linear formulation above)
*
* \f[
*   P_casting = P_target
* \f]
* 
* \f[
*   d = d_casting + d_target;
* \f]
*
* Here, the first two equations use the Bei and Fregly elastic foundation model
* to define the relationship between the local mesh properties, local overlap 
* depth and computed pressure. The third equation is a force equilibrium, 
* assuming that the force applied to a pair of contacting triangles is 
* equal and opposite. This formulation further assumes that the triangles in 
* contact have the same area. The fourth equation states that the total overlap
* depth of the meshes (which is readily calculated) is distributed between the 
* local overlap depths of the two elastic layes in contact. 
*
* This system of equations can be solved analytically if the linear Pressure-
* depth relationship is used. If the non-linear relationship is used, the 
* system of equations is solved using numerical techniques. 
*
*
*
*/

	 //class Smith2018ArticularContactForce : public Force {
    class OSIMPLUGIN_API Smith2018ArticularContactForce : public Force {
        OpenSim_DECLARE_CONCRETE_OBJECT(Smith2018ArticularContactForce, Force)

            struct contact_stats;

    public:
        class ContactParameters;


        //=====================================================================
        // PROPERTIES
        //=====================================================================
        OpenSim_DECLARE_PROPERTY(min_proximity, double, "Minimum overlap depth"
            "between contacting meshes")
            OpenSim_DECLARE_PROPERTY(max_proximity, double, "Maximum overlap depth"
                "between contacting meshes")
            OpenSim_DECLARE_PROPERTY(elastic_foundation_formulation, std::string,
                "Formulation for depth-pressure relationship: "
                "'linear' or 'nonlinear'")
            OpenSim_DECLARE_PROPERTY(use_lumped_contact_model, bool,
                "Combine the thickness and average material properties between "
                "the ContactParams for both meshes and use Bei & Fregly 2003"
                " lumped parameter Elastic Foundation model")
            OpenSim_DECLARE_OPTIONAL_PROPERTY(verbose, int, "Level of reporting"
                " for debugging purposes (0-silent, 1-simple, 2-detailed)")
            OpenSim_DECLARE_PROPERTY(target_mesh_contact_params,
                Smith2018ArticularContactForce::ContactParameters,
                "target_mesh material properties")
            OpenSim_DECLARE_PROPERTY(casting_mesh_contact_params,
                Smith2018ArticularContactForce::ContactParameters,
                "casting_mesh material properties")


            //=====================================================================
            // Connectors
            //=====================================================================
            OpenSim_DECLARE_SOCKET(target_mesh, Smith2018ContactMesh,
                "Target mesh for collision detection.")
            OpenSim_DECLARE_SOCKET(casting_mesh, Smith2018ContactMesh,
                "Ray casting mesh for collision detection.")


            //==============================================================================
            // OUTPUTS
            //==============================================================================
            //number of colliding triangles
            OpenSim_DECLARE_OUTPUT(target_total_n_colliding_tri, int,
                getTargetNContactingTri, SimTK::Stage::Dynamics)
            OpenSim_DECLARE_OUTPUT(casting_total_n_collinding_tri, int,
                getCastingNContactingTri, SimTK::Stage::Dynamics)

            //tri proximity
            OpenSim_DECLARE_OUTPUT(target_tri_proximity, SimTK::Vector,
                getTargetTriProximity, SimTK::Stage::Position)
            OpenSim_DECLARE_OUTPUT(casting_tri_proximity, SimTK::Vector,
                getCastingTriProximity, SimTK::Stage::Position)

            //tri pressure
            OpenSim_DECLARE_OUTPUT(target_tri_pressure, SimTK::Vector,
                getTargetTriPressure, SimTK::Stage::Dynamics)
            OpenSim_DECLARE_OUTPUT(casting_tri_pressure, SimTK::Vector,
                getCastingTriPressure, SimTK::Stage::Dynamics)

            //tri potential energy
            OpenSim_DECLARE_OUTPUT(target_tri_potential_energy, SimTK::Vector,
                getTargetTriPotentialEnergy, SimTK::Stage::Dynamics)
            OpenSim_DECLARE_OUTPUT(casting_tri_potential_energy, SimTK::Vector,
                getCastingTriPotentialEnergy, SimTK::Stage::Dynamics)

            //contact_area
            OpenSim_DECLARE_OUTPUT(target_total_contact_area, double,
                getTargetContactArea, SimTK::Stage::Position)
            OpenSim_DECLARE_OUTPUT(casting_total_contact_area, double,
                getCastingContactArea, SimTK::Stage::Position)

            OpenSim_DECLARE_OUTPUT(target_regional_contact_area, SimTK::Vector,
                getTargetRegionalContactArea, SimTK::Stage::Position)
            OpenSim_DECLARE_OUTPUT(casting_regional_contact_area, SimTK::Vector,
                getCastingRegionalContactArea, SimTK::Stage::Position)

            //mean proximity
            OpenSim_DECLARE_OUTPUT(target_total_mean_proximity, double,
                getTargetMeanProximity, SimTK::Stage::Position)
            OpenSim_DECLARE_OUTPUT(casting_total_mean_proximity, double,
                getCastingMeanProximity, SimTK::Stage::Position)

            OpenSim_DECLARE_OUTPUT(target_regional_mean_proximity, SimTK::Vector,
                getTargetRegionalMeanProximity, SimTK::Stage::Position)
            OpenSim_DECLARE_OUTPUT(casting_regional_mean_proximity, SimTK::Vector,
                getCastingRegionalMeanProximity, SimTK::Stage::Position)

            //max proximity
            OpenSim_DECLARE_OUTPUT(target_total_max_proximity, double,
                getTargetMaxProximity, SimTK::Stage::Position)
            OpenSim_DECLARE_OUTPUT(casting_total_max_proximity, double,
                getCastingMaxProximity, SimTK::Stage::Position)

            OpenSim_DECLARE_OUTPUT(target_regional_max_proximity, SimTK::Vector,
                getTargetRegionalMaxProximity, SimTK::Stage::Position)
            OpenSim_DECLARE_OUTPUT(casting_regional_max_proximity, SimTK::Vector,
                getCastingRegionalMaxProximity, SimTK::Stage::Position)

            //mean pressure
            OpenSim_DECLARE_OUTPUT(target_total_mean_pressure, double,
                getTargetMeanPressure, SimTK::Stage::Dynamics)
            OpenSim_DECLARE_OUTPUT(casting_total_mean_pressure, double,
                getCastingMeanPressure, SimTK::Stage::Dynamics)

            OpenSim_DECLARE_OUTPUT(target_regional_mean_pressure, SimTK::Vector,
                getTargetRegionalMeanPressure, SimTK::Stage::Dynamics)
            OpenSim_DECLARE_OUTPUT(casting_regional_mean_pressure, SimTK::Vector,
                getCastingRegionalMeanPressure, SimTK::Stage::Dynamics)

            //max pressure
            OpenSim_DECLARE_OUTPUT(target_total_max_pressure, double,
                getTargetMaxPressure, SimTK::Stage::Dynamics)
            OpenSim_DECLARE_OUTPUT(casting_total_max_pressure, double,
                getCastingMaxPressure, SimTK::Stage::Dynamics)

            OpenSim_DECLARE_OUTPUT(target_regional_max_pressure, SimTK::Vector,
                getTargetRegionalMaxPressure, SimTK::Stage::Dynamics)
            OpenSim_DECLARE_OUTPUT(casting_regional_max_pressure, SimTK::Vector,
                getCastingRegionalMaxPressure, SimTK::Stage::Dynamics)

            //center of proximity
            OpenSim_DECLARE_OUTPUT(target_total_center_of_proximity, double,
                getTargetCenterOfProximity, SimTK::Stage::Position)
            OpenSim_DECLARE_OUTPUT(casting_total_center_of_proximity, double,
                getCastingCenterOfProximity, SimTK::Stage::Position)

            OpenSim_DECLARE_OUTPUT(target_regional_center_of_proximity,
                SimTK::Vector, getTargetRegionalCenterOfProximity,
                SimTK::Stage::Position)
            OpenSim_DECLARE_OUTPUT(casting_regional_center_of_proximity,
                SimTK::Vector, getCastingRegionalCenterOfProximity,
                SimTK::Stage::Position)

            //center of pressure
            OpenSim_DECLARE_OUTPUT(target_total_center_of_pressure, double,
                getTargetCenterOfPressure, SimTK::Stage::Dynamics)
            OpenSim_DECLARE_OUTPUT(casting_total_center_of_pressure, double,
                getCastingCenterOfPressure, SimTK::Stage::Dynamics)

            OpenSim_DECLARE_OUTPUT(target_regional_center_of_pressure,
                SimTK::Vector, getTargetRegionalCenterOfPressure,
                SimTK::Stage::Dynamics)
            OpenSim_DECLARE_OUTPUT(casting_regional_center_of_pressure,
                SimTK::Vector, getCastingRegionalCenterOfPressure,
                SimTK::Stage::Dynamics)

            //contact force
            OpenSim_DECLARE_OUTPUT(target_total_contact_force, SimTK::Vec3,
                getTargetContactForce, SimTK::Stage::Dynamics)
            OpenSim_DECLARE_OUTPUT(casting_total_contact_force, SimTK::Vec3,
                getCastingContactForce, SimTK::Stage::Dynamics)

            OpenSim_DECLARE_OUTPUT(target_regional_contact_force,
                SimTK::Vector_<SimTK::Vec3>, getTargetRegionalContactForce,
                SimTK::Stage::Dynamics)
            OpenSim_DECLARE_OUTPUT(casting_regional_contact_force,
                SimTK::Vector_<SimTK::Vec3>, getCastingRegionalContactForce,
                SimTK::Stage::Dynamics)

            //contact moment
            OpenSim_DECLARE_OUTPUT(target_total_contact_moment, SimTK::Vec3,
                getTargetContactMoment, SimTK::Stage::Dynamics)
            OpenSim_DECLARE_OUTPUT(casting_total_contact_moment, SimTK::Vec3,
                getCastingContactMoment, SimTK::Stage::Dynamics)

            OpenSim_DECLARE_OUTPUT(target_regional_contact_moment,
                SimTK::Vector_<SimTK::Vec3>, getTargetRegionalContactMoment,
                SimTK::Stage::Dynamics)
            OpenSim_DECLARE_OUTPUT(casting_regional_contact_moment,
                SimTK::Vector_<SimTK::Vec3>, getCastingRegionalContactMoment,
                SimTK::Stage::Dynamics)

            //=====================================================================
            //METHODS
            //=====================================================================

            Smith2018ArticularContactForce();

        Smith2018ArticularContactForce(
            Smith2018ContactMesh& target_mesh,
            Smith2018ArticularContactForce::ContactParameters
            target_mesh_params, Smith2018ContactMesh& casting_mesh,
            Smith2018ArticularContactForce::ContactParameters
            casting_mesh_params, int verbose = 0);


    public:

        //---------------------------------------------------------------------
        //Output Methods
        //---------------------------------------------------------------------

        //number of contacting triangles
        int getTargetNContactingTri(const SimTK::State& state) const {
            return getCacheVariableValue<int>
                (state, "target.n_contacting_tri");
        }

        int getCastingNContactingTri(const SimTK::State& state) const {
            return getCacheVariableValue<int>
                (state, "casting.n_contacting_tri");
        }

        //tri proximity
        SimTK::Vector getTargetTriProximity(const SimTK::State& state) const {
            return getCacheVariableValue<SimTK::Vector>
                (state, "target.tri.proximity");
        }
        SimTK::Vector getCastingTriProximity(const SimTK::State& state) const {
            return getCacheVariableValue<SimTK::Vector>
                (state, "casting.tri.proximity");
        }

        //tri pressure
        SimTK::Vector getTargetTriPressure(const SimTK::State& state) const {
            return getCacheVariableValue<SimTK::Vector>
                (state, "target.tri.pressure");
        }
        SimTK::Vector getCastingTriPressure(const SimTK::State& state) const {
            return getCacheVariableValue<SimTK::Vector>
                (state, "casting.tri.pressure");
        }

        //tri potential energy
        SimTK::Vector getTargetTriPotentialEnergy(const SimTK::State& state) const {
            return getCacheVariableValue<SimTK::Vector>
                (state, "target.tri.potential_energy");
        }
        SimTK::Vector getCastingTriPotentialEnergy(const SimTK::State& state) const {
            return getCacheVariableValue<SimTK::Vector>
                (state, "casting.tri.potential_energy");
        }

        //contact_area
        double getTargetContactArea(const SimTK::State& state) const {
            return getCacheVariableValue<double>
                (state, "target.contact_area");
        }
        double getCastingContactArea(const SimTK::State& state) const {
            return getCacheVariableValue<double>
                (state, "casting.contact_area");
        }

        SimTK::Vector getTargetRegionalContactArea(const SimTK::State& state) const {
            return getCacheVariableValue<SimTK::Vector>
                (state, "target.regional.contact_area");
        }
        SimTK::Vector getCastingRegionalContactArea(const SimTK::State& state) const {
            return getCacheVariableValue<SimTK::Vector>
                (state, "casting.regional.contact_area");
        }

        //mean proximity
        double getTargetMeanProximity(const SimTK::State& state) const {
            return getCacheVariableValue<double>
                (state, "target.mean_proximity");
        }
        double getCastingMeanProximity(const SimTK::State& state) const {
            return getCacheVariableValue<double>
                (state, "casting.mean_proximity");
        }

        SimTK::Vector getTargetRegionalMeanProximity(const SimTK::State& state) const {
            return getCacheVariableValue<SimTK::Vector>
                (state, "target.regional.mean_proximity");
        }
        SimTK::Vector getCastingRegionalMeanProximity(const SimTK::State& state) const {
            return getCacheVariableValue<SimTK::Vector>
                (state, "casting.regional.mean_proximity");
        }

        //max proximity
        double getTargetMaxProximity(const SimTK::State& state) const {
            return getCacheVariableValue<double>
                (state, "target.max_proximity");
        }
        double getCastingMaxProximity(const SimTK::State& state) const {
            return getCacheVariableValue<double>
                (state, "casting.max_proximity");
        }

        SimTK::Vector getTargetRegionalMaxProximity(const SimTK::State& state) const {
            return getCacheVariableValue<SimTK::Vector>
                (state, "target.regional.max_proximity");
        }
        SimTK::Vector getCastingRegionalMaxProximity(const SimTK::State& state) const {
            return getCacheVariableValue<SimTK::Vector>
                (state, "casting.regional.max_proximity");
        }

        //mean pressure
        double getTargetMeanPressure(const SimTK::State& state) const {
            return getCacheVariableValue<double>
                (state, "target.mean_pressure");
        }
        double getCastingMeanPressure(const SimTK::State& state) const {
            return getCacheVariableValue<double>
                (state, "casting.mean_pressure");
        }

        SimTK::Vector getTargetRegionalMeanPressure(const SimTK::State& state) const {
            return getCacheVariableValue<SimTK::Vector>
                (state, "target.regional.mean_pressure");
        }
        SimTK::Vector getCastingRegionalMeanPressure(const SimTK::State& state) const {
            return getCacheVariableValue<SimTK::Vector>
                (state, "casting.regional.mean_pressure");
        }

        //max pressure
        double getTargetMaxPressure(const SimTK::State& state) const {
            return getCacheVariableValue<double>
                (state, "target.max_pressure");
        }
        double getCastingMaxPressure(const SimTK::State& state) const {
            return getCacheVariableValue<double>
                (state, "casting.max_pressure");
        }

        SimTK::Vector getTargetRegionalMaxPressure(const SimTK::State& state) const {
            return getCacheVariableValue<SimTK::Vector>
                (state, "target.regional.max_pressure");
        }
        SimTK::Vector getCastingRegionalMaxPressure(const SimTK::State& state) const {
            return getCacheVariableValue<SimTK::Vector>
                (state, "casting.regional.max_pressure");
        }

        //center of proximity
        double getTargetCenterOfProximity(const SimTK::State& state) const {
            return getCacheVariableValue<double>
                (state, "target.center_of_proximity");
        }
        double getCastingCenterOfProximity(const SimTK::State& state) const {
            return getCacheVariableValue<double>
                (state, "casting.center_of_proximity");
        }

        SimTK::Vector getTargetRegionalCenterOfProximity(const SimTK::State& state) const {
            return getCacheVariableValue<SimTK::Vector>
                (state, "target.regional.center_of_proximity");
        }
        SimTK::Vector getCastingRegionalCenterOfProximity(const SimTK::State& state) const {
            return getCacheVariableValue<SimTK::Vector>
                (state, "casting.regional.center_of_proximity");
        }

        //center of pressure
        double getTargetCenterOfPressure(const SimTK::State& state) const {
            return getCacheVariableValue<double>
                (state, "target.center_of_pressure");
        }
        double getCastingCenterOfPressure(const SimTK::State& state) const {
            return getCacheVariableValue<double>
                (state, "casting.center_of_pressure");
        }

        SimTK::Vector getTargetRegionalCenterOfPressure(const SimTK::State& state) const {
            return getCacheVariableValue<SimTK::Vector>
                (state, "target.regional.center_of_pressure");
        }
        SimTK::Vector getCastingRegionalCenterOfPressure(const SimTK::State& state) const {
            return getCacheVariableValue<SimTK::Vector>
                (state, "casting.regional.center_of_pressure");
        }

        //contact force
        SimTK::Vec3 getTargetContactForce(const SimTK::State& state) const {
            return getCacheVariableValue<SimTK::Vec3>
                (state, "target.contact_force");
        }
        SimTK::Vec3 getCastingContactForce(const SimTK::State& state) const {
            return getCacheVariableValue<SimTK::Vec3>
                (state, "casting.contact_force");
        }

        SimTK::Vector_<SimTK::Vec3> getTargetRegionalContactForce(const SimTK::State& state) const {
            return getCacheVariableValue<SimTK::Vector_<SimTK::Vec3>>
                (state, "target.regional.contact_force");
        }
        SimTK::Vector_<SimTK::Vec3> getCastingRegionalContactForce(const SimTK::State& state) const {
            return getCacheVariableValue<SimTK::Vector_<SimTK::Vec3>>
                (state, "casting.regional.contact_force");
        }

        //contact moment
        SimTK::Vec3 getTargetContactMoment(const SimTK::State& state) const {
            return getCacheVariableValue<SimTK::Vec3>
                (state, "target.contact_moment");
        }
        SimTK::Vec3 getCastingContactMoment(const SimTK::State& state) const {
            return getCacheVariableValue<SimTK::Vec3>
                (state, "casting.contact_moment");
        }

        SimTK::Vector_<SimTK::Vec3> getTargetRegionalContactMoment(
            const SimTK::State& state) const {
            return getCacheVariableValue<SimTK::Vector_<SimTK::Vec3>>
                (state, "target.regional.contact_moment");
        }
        SimTK::Vector_<SimTK::Vec3> getCastingRegionalContactMoment(
            const SimTK::State& state) const {
            return getCacheVariableValue<SimTK::Vector_<SimTK::Vec3>>
                (state, "casting.regional.contact_moment");
        }

        OpenSim::Array<double> getRecordValues(const SimTK::State& s) const;
        OpenSim::Array<std::string> getRecordLabels() const;

    protected:
        double computePotentialEnergy(
            const SimTK::State& state) const override;

        void computeForce(const SimTK::State& state,
            SimTK::Vector_<SimTK::SpatialVec>& bodyForces,
            SimTK::Vector& generalizedForces) const override;

        void extendAddToSystem(SimTK::MultibodySystem& system) const override;
        void extendInitStateFromProperties(SimTK::State& state)	const override;
        void extendRealizeReport(const SimTK::State & state)	const override;

        void computeTriProximity(const SimTK::State& state,
            const Smith2018ContactMesh& casting_mesh,
            const Smith2018ContactMesh& target_mesh,
            const std::string& cache_mesh_name) const;

        void computeTriDynamics(const SimTK::State& state,
            const Smith2018ContactMesh& casting_mesh,
            const Smith2018ContactMesh& target_mesh,
            const std::string& cache_mesh_name,
            SimTK::Vector_<SimTK::SpatialVec>& bodyForces,
            SimTK::Vector_<SimTK::Vec3>& tri_force) const;

        SimTK::Vec3 computeContactForceVector(
            double pressure, double area, SimTK::Vec3 normal) const;

        SimTK::Vec3 computeContactMomentVector(
            double pressure, double area, SimTK::Vec3 normal,
            SimTK::Vec3 center) const;

        contact_stats computeContactStats(const SimTK::State& state,
            const std::string& mesh_type, const std::vector<int>& triIndices) const;

    private:
        void setNull();
        void constructProperties();

        static void calcNonlinearPressureResid(
            int nEqn, int nVar, double q[], double resid[],
            int *flag2, void *ptr);

        //=====================================================================
        //Member Variables
        //=====================================================================
        struct nonlinearContactParams {
            double h1, h2, k1, k2, dc;
        };

        struct contact_stats
        {
            double contact_area;
            double mean_proximity;
            double max_proximity;
            SimTK::Vec3 center_of_proximity;
            double mean_pressure;
            double max_pressure;
            SimTK::Vec3 center_of_pressure;
            SimTK::Vec3 contact_force;
            SimTK::Vec3 contact_moment;
        };

        std::vector<std::string> _region_names;
        std::vector<std::string> _stat_names;
        std::vector<std::string> _stat_names_vec3;
        std::vector<std::string> _mesh_data_names;
    };		
    //=========================================================================
    // END of class Smith2018ArticularContactForce
	//=========================================================================


    //=========================================================================
    //              Smith2018ArticularContactForce :: CONTACT PARAMETERS
    //=========================================================================
	//class Smith2018ArticularContactForce::ContactParameters : public Object {
    class OSIMPLUGIN_API Smith2018ArticularContactForce::ContactParameters :
        public Object { OpenSim_DECLARE_CONCRETE_OBJECT(
            Smith2018ArticularContactForce::ContactParameters, Object)
        
    public:
        //=====================================================================
        // PROPERTIES
        //=====================================================================
			OpenSim_DECLARE_PROPERTY(use_variable_thickness, bool,
				"Flag to use variable thickness."
				"Note: mesh_back_file must defined in Smith2018ContactMesh")
			OpenSim_DECLARE_PROPERTY(use_variable_elastic_modulus, bool,
				"Flag to use variable youngs modulus."
				"Note: material_properties_file must defined in " 
                "Smith2018ContactMesh")
			OpenSim_DECLARE_PROPERTY(use_variable_poissons_ratio, bool,
				"Flag to use variable poissons ratio."
                "Note: material_properties_file must defined in " 
                "Smith2018ContactMesh")
			OpenSim_DECLARE_PROPERTY(elastic_modulus, double, 
                "Uniform Elastic Modulus value for entire mesh")
			OpenSim_DECLARE_PROPERTY(poissons_ratio, double, 
                "Uniform Poissons Ratio value for entire mesh")
			OpenSim_DECLARE_PROPERTY(thickness, double,
			    "Uniform thickness of elastic layer for entire mesh")
            //=================================================================
            // METHODS
            //=================================================================
            ContactParameters();
            ContactParameters(double youngs_modulus, double poissons_ratio,
                double thickness);


     private:
        void constructProperties();
    };


} // end of namespace OpenSim

#endif // OPENSIM_SMITH_2018_ARTICULAR_CONTACT_FORCE_H_
