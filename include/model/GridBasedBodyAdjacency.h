#ifndef DWL__MODEL__GRID_BASED_BODY_ADJACENCY__H
#define DWL__MODEL__GRID_BASED_BODY_ADJACENCY__H

#include <dwl/model/AdjacencyModel.h>
#include <dwl/robot/Robot.h>
#include <dwl/environment/TerrainMap.h>
#include <dwl/environment/Feature.h>


namespace dwl
{

namespace model
{

/**
 * @class GridBasedBodyAdjacency
 * @brief Class for building a grid-based body adjacency map of the environment. This class derives from
 * AdjacencyEnvironment class
 */
class GridBasedBodyAdjacency : public AdjacencyModel
{
	public:
		/** @brief Constructor function */
		GridBasedBodyAdjacency();

		/** @brief Destructor function */
		~GridBasedBodyAdjacency();

		/**
		 * @brief Defines the environment information
		 * @param Robot* Encapsulated all the robot information
		 * @param TerrainMap* Encapsulates all the terrain information
		 */
		void reset(robot::Robot* robot,
				   environment::TerrainMap* environment);

		/**
		 * @brief Computes the whole adjacency map
		 * @param AdjacencyMap& Adjacency map
		 * @param Vertex Source vertex
		 * @param Vertex Target vertex
		 */
		void computeAdjacencyMap(AdjacencyMap& adjacency_map,
								 Vertex source,
								 Vertex target);

		/**
		 * @brief Gets the successors of the current vertex
		 * @param std::list<Edge>& List of successors
		 * @param Vertex Current state vertex
		 */
		void getSuccessors(std::list<Edge>& successors,
						   Vertex state_vertex);


	private:
		/**
		  * @brief Gets the closest start and goal vertex if it is not belong to
		  * the terrain information
		  * @param Vertex& The closest vertex to the start
		  * @param Vertex& The closest vertex to the goal
		  * @param Vertex Start vertex
		  * @param Vertex Goal vertex
		  */
		void getTheClosestStartAndGoalVertex(Vertex& closest_source,
											 Vertex& closest_target,
											 Vertex source,
											 Vertex target);

		/**
		 * @brief Searches the neighbors of a current vertex
		 * @param std::vector<Vertex>& The set of states neighbors
		 * @param Vertex Current state vertex
		 */
		void searchNeighbors(std::vector<Vertex>& neighbor_states,
							 Vertex state_vertex);

		/**
		 * @brief Computes the body cost of a current vertex
		 * @param Vertex Current state vertex
		 */
		void computeBodyCost(double& cost,
							 Vertex state_vertex);

		/** @brief Asks if it is requested a stance adjacency */
		bool isStanceAdjacency();


		/** @brief Pointer to robot properties */
		robot::Robot* robot_;

		/** @brief Pointer of the TerrainMap object which describes the terrain */
		environment::TerrainMap* terrain_;

		/** @brief Vector of pointers to the Feature class */
		std::vector<environment::Feature*> features_;

		/** @brief Indicates it was requested a stance or terrain adjacency */
		bool is_stance_adjacency_;

		/** @brief A Map of search areas */
		SearchAreaMap stance_areas_;

		/** @brief Definition of the neighboring area (number of neighbors per size) */
		int neighboring_definition_;

		/** @brief Number of top cost for computing the stance cost */
		int number_top_cost_;

		/** @brief Uncertainty factor which is applied in non-perceived environment */
		double uncertainty_factor_; // For unknown (non-perceive) areas
};

} //@namespace model
} //@namespace dwl

#endif
