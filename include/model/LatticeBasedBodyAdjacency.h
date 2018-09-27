#ifndef DWL__MODEL__LATTICE_BASED_BODY_ADJACENCY__H
#define DWL__MODEL__LATTICE_BASED_BODY_ADJACENCY__H

#include <dwl/model/AdjacencyModel.h>
#include <dwl/robot/Robot.h>
#include <dwl/environment/TerrainMap.h>
#include <dwl/environment/Feature.h>



namespace dwl
{

namespace model
{

/**
 * @class LatticeBasedBodyAdjacency
 * @brief Class for building a lattice-based adjacency map of the environment. This class derives
 * from AdjacencyModel class
 */
class LatticeBasedBodyAdjacency : public AdjacencyModel
{
	public:
		/** @brief Constructor function */
		LatticeBasedBodyAdjacency();

		/** @brief Destructor function */
		~LatticeBasedBodyAdjacency();

		/**
		 * @brief Defines the settings of all components within AdjacencyModel class
		 * @param robot::Robot* The robot defines all the properties of the robot
		 * @param environment::TerrainMap* Pointer to object that describes the
		 * terrain environment
		 */
		void reset(robot::Robot* robot,
				   environment::TerrainMap* environment);

		/**
		 * @brief Gets the successors of the current vertex
		 * @param std::list<Edge>& List of successors
		 * @param Vertex Current state vertex
		 */
		void getSuccessors(std::list<Edge>& successors,
						   Vertex state_vertex);


	private:
		/**
		 * @brief Searches the neighbors of a current vertex
		 * @param std::vector<Vertex>& The set of neighbors
		 * @param Vertex Current vertex
		 */
		void searchNeighbors(std::vector<Vertex>& neighbors,
							 Vertex vertex_id);

		/**
		 * @brief Computes the body cost of a current vertex
		 * @param Eigen::Vector3d Current robot state (x,y,yaw)
		 */
		void computeBodyCost(double& cost,
							 Eigen::Vector3d state);

		/**
		 * @brief Indicates if the free of obstacle
		 * @param Vertex State vertex
		 * @param TypeOfState State representation
		 * @param bool Indicates it is desired to use the body space definition
		 * @return True if it is free of obstacle, and false otherwise
		 */
		bool isFreeOfObstacle(Vertex state_vertex,
							  TypeOfState state_representation,
							  bool body=false);

		/**
		 * @brief Indicates if it is requested a stance adjacency
		 * @return True it is requested a stance adjacency (body cost), false otherwise
		 */
		bool isStanceAdjacency();

		/** @brief Pointer to robot properties */
		robot::Robot* robot_;

		/** @brief Pointer of the TerrainMap object which describes the terrain */
		environment::TerrainMap* terrain_;

		/** @brief Vector of pointers to the Feature class */
		std::vector<environment::Feature*> features_;

		/** @brief Current action of the body */
		Eigen::Vector3d current_action_;

		/** @brief Indicates it was requested a stance or terrain adjacency */
		bool is_stance_adjacency_;

		/** @brief Map of search areas */
		SearchAreaMap stance_areas_;

		/** @brief Number of top cost for computing the stance cost */
		int number_top_cost_;

		/** @brief Uncertainty factor which is applied in unperceived environment */
		double uncertainty_factor_; // For unknown (non-perceive) areas
};

} //@namespace model
} //@namespace dwl

#endif
