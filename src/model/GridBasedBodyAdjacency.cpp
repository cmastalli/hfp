#include <dwl/model/GridBasedBodyAdjacency.h>


namespace dwl
{

namespace model
{

GridBasedBodyAdjacency::GridBasedBodyAdjacency() : robot_(NULL),
		terrain_(NULL), is_stance_adjacency_(true),
		neighboring_definition_(3), number_top_cost_(5),
		uncertainty_factor_(1.15)
{
	name_ = "Grid-based Body";
	is_lattice_ = false;
}


GridBasedBodyAdjacency::~GridBasedBodyAdjacency()
{

}


void GridBasedBodyAdjacency::reset(robot::Robot* robot,
						   environment::TerrainMap* environment)
{
	printf(BLUE "Setting the robot information in the %s adjacency model \n"
			COLOR_RESET, name_.c_str());
	robot_ = robot;

	printf(BLUE "Setting the environment information in the %s adjacency model"
			" \n" COLOR_RESET, name_.c_str());
	terrain_ = environment;

	for (int i = 0; i < (int) features_.size(); i++)
		features_[i]->reset(robot);
}


void GridBasedBodyAdjacency::computeAdjacencyMap(AdjacencyMap& adjacency_map,
												 Vertex source,
												 Vertex target)
{
	// Computing a default stance areas
	Eigen::Vector3d full_action = Eigen::Vector3d::Zero();
	stance_areas_ = robot_->getFootstepSearchAreas(full_action);

	// Getting the body orientation
	Eigen::Vector3d initial_state;
	terrain_->getTerrainSpaceModel().vertexToState(initial_state, source);
	unsigned short int key_yaw;
	terrain_->getTerrainSpaceModel().stateToKey(key_yaw, (double) initial_state(2), false);
	double yaw;
	terrain_->getTerrainSpaceModel().keyToState(yaw, key_yaw, false);

	if (terrain_->isTerrainInformation()) {
		// Adding the source and target vertex if it is outside the information terrain
		Vertex closest_source, closest_target;
		getTheClosestStartAndGoalVertex(closest_source, closest_target, source, target);
		if (closest_source != source) {
			adjacency_map[source].push_back(Edge(closest_source, 0));
		}
		if (closest_target != target) {
			adjacency_map[closest_target].push_back(Edge(target, 0));
		}

		// Computing the adjacency map given the terrain information
		TerrainDataMap terrain_map = terrain_->getTerrainDataMap();
		for (TerrainDataMap::iterator vertex_iter = terrain_map.begin();
				vertex_iter != terrain_map.end();
				vertex_iter++)
		{
			Vertex vertex = vertex_iter->first;
			Eigen::Vector2d current_coord;

			Vertex state_vertex;
			terrain_->getTerrainSpaceModel().vertexToCoord(current_coord, vertex);
			Eigen::Vector3d current_state;
			current_state << current_coord, yaw;
			terrain_->getTerrainSpaceModel().stateToVertex(state_vertex, current_state);

			if (!isStanceAdjacency()) {
				double terrain_cost = vertex_iter->second.cost;

				// Searching the neighbor actions
				std::vector<Vertex> neighbor_actions;
				searchNeighbors(neighbor_actions, state_vertex);
				for (unsigned int i = 0; i < neighbor_actions.size(); i++)
					adjacency_map[neighbor_actions[i]].push_back(Edge(state_vertex, terrain_cost));
			} else {
				// Computing the body cost
				double body_cost;
				computeBodyCost(body_cost, state_vertex);

				// Searching the neighbor actions
				std::vector<Vertex> neighbor_actions;
				searchNeighbors(neighbor_actions, state_vertex);
				for (unsigned int i = 0; i < neighbor_actions.size(); i++)
					adjacency_map[neighbor_actions[i]].push_back(Edge(state_vertex, body_cost));
			}
		}
	} else
		printf(RED "Could not computed the adjacency map because there is not"
				" terrain information \n" COLOR_RESET);
}


void GridBasedBodyAdjacency::getSuccessors(std::list<Edge>& successors,
										   Vertex state_vertex)
{
	Eigen::Vector3d state;
	terrain_->getTerrainSpaceModel().vertexToState(state, state_vertex);

	std::vector<Vertex> neighbor_actions;
	searchNeighbors(neighbor_actions, state_vertex);
	if (terrain_->isTerrainInformation()) {
		// Getting the terrain map
		TerrainDataMap terrain_map = terrain_->getTerrainDataMap();

		unsigned int action_size = neighbor_actions.size();
		for (unsigned int i = 0; i < action_size; i++) {
			// Converting the state vertex (x,y,yaw) to a terrain vertex (x,y)
			Vertex terrain_vertex;
			terrain_->getTerrainSpaceModel().stateVertexToEnvironmentVertex(
					terrain_vertex,	neighbor_actions[i], XY_Y);

			if (!isStanceAdjacency()) {
				double terrain_cost = terrain_->getTerrainCost(terrain_vertex);
				successors.push_back(Edge(neighbor_actions[i], terrain_cost));
			} else {
				// Computing the body cost
				double body_cost;
				computeBodyCost(body_cost, neighbor_actions[i]);
				successors.push_back(Edge(neighbor_actions[i], body_cost));
			}
		}
	} else
		printf(RED "Could not computed the successors because there is not"
				" terrain information \n" COLOR_RESET);
}


void GridBasedBodyAdjacency::getTheClosestStartAndGoalVertex(Vertex& closest_source,
															 Vertex& closest_target,
															 Vertex source,
															 Vertex target)
{
	// Checking if the start and goal vertex are part of the terrain information
	bool is_there_start_vertex, is_there_goal_vertex = false;
	std::vector<Vertex> vertex_map;
	if (terrain_->isTerrainInformation()) {
		TerrainDataMap terrain_map = terrain_->getTerrainDataMap();
		for (TerrainDataMap::iterator vertex_iter = terrain_map.begin();
				vertex_iter != terrain_map.end(); vertex_iter++) {
			Vertex current_vertex = vertex_iter->first;
			if (source == current_vertex) {
				is_there_start_vertex = true;
				closest_source = current_vertex;
			}
			if (target == current_vertex) {
				is_there_goal_vertex = true;
				closest_target = current_vertex;
			}

			if ((is_there_start_vertex) && (is_there_goal_vertex))
				return;

			vertex_map.push_back(vertex_iter->first);
		}
	} else {
		printf(RED "Could not get the closest start and goal vertex because"
				" there is not terrain information \n" COLOR_RESET);
		return;
	}

	// Start and goal state
	Eigen::Vector3d start_state, goal_state;
	terrain_->getTerrainSpaceModel().vertexToState(start_state, source);
	terrain_->getTerrainSpaceModel().vertexToState(goal_state, target);

	double closest_source_distant = std::numeric_limits<double>::max();
	double closest_target_distant = std::numeric_limits<double>::max();
	Vertex start_closest_vertex = 0, goal_closest_vertex = 0;
	if ((!is_there_start_vertex) && (!is_there_goal_vertex)) {
		Eigen::Vector3d current_state;
		for (unsigned int i = 0; i < vertex_map.size(); i++) {
			// Calculating the vertex position
			terrain_->getTerrainSpaceModel().vertexToState(current_state, vertex_map[i]);

			// Calculating the distant to the vertex from start and goal positions
			double start_distant = (start_state.head(2) - current_state.head(2)).norm();
			double goal_distant = (goal_state.head(2) - current_state.head(2)).norm();

			// Recording the closest vertex from the start position
			if (start_distant < closest_source_distant) {
				start_closest_vertex = vertex_map[i];
				closest_source_distant = start_distant;
			}

			// Recording the closest vertex from the goal position
			if (goal_distant < closest_target_distant) {
				goal_closest_vertex = vertex_map[i];
				closest_target_distant = goal_distant;
			}
		}

		// Adding the goal to the adjacency map
		closest_source = start_closest_vertex;
		closest_target = goal_closest_vertex;
	} else if (!is_there_start_vertex) {
		Eigen::Vector3d current_state;
		for (unsigned int i = 0; i < vertex_map.size(); i++) {
			// Calculating the vertex position
			terrain_->getTerrainSpaceModel().vertexToState(current_state, vertex_map[i]);

			// Calculating the distant to the vertex from start position
			double start_distant = (start_state.head(2) - current_state.head(2)).norm();

			// Recording the closest vertex from the start position
			if (start_distant < closest_source_distant) {
				start_closest_vertex = vertex_map[i];
				closest_source_distant = start_distant;
			}
		}

		// Adding the start to the adjacency map
		closest_source = start_closest_vertex;
	} else if (!is_there_goal_vertex) {
		Eigen::Vector3d current_state;
		for (unsigned int i = 0; i < vertex_map.size(); i++) {
			// Calculating the vertex position
			terrain_->getTerrainSpaceModel().vertexToState(current_state, vertex_map[i]);

			// Calculating the distant to the vertex from goal position
			double goal_distant = (goal_state.head(2) - current_state.head(2)).norm();

			// Recording the closest vertex from the goal position
			if (goal_distant < closest_target_distant) {
				goal_closest_vertex = vertex_map[i];
				closest_target_distant = goal_distant;
			}
		}

		// Adding the goal to the adjacency map
		closest_target = goal_closest_vertex;
	}
}


void GridBasedBodyAdjacency::searchNeighbors(std::vector<Vertex>& neighbor_states,
											 Vertex state_vertex)
{
	// Getting the key of yaw
	unsigned short int key_yaw;
	Eigen::Vector3d state;
	terrain_->getTerrainSpaceModel().vertexToState(state, state_vertex);
	terrain_->getTerrainSpaceModel().stateToKey(key_yaw, (double) state(2), false);

	// Getting the key for x and y axis
	Key terrain_key;
	Vertex terrain_vertex;
	terrain_->getTerrainSpaceModel().stateVertexToEnvironmentVertex(terrain_vertex, state_vertex, XY_Y);
	terrain_->getTerrainSpaceModel().vertexToKey(terrain_key, terrain_vertex, true);

	// Searching the closed neighbors around 3-neighboring area
	bool is_found_neighbor_positive_x = false, is_found_neighbor_negative_x = false;
	bool is_found_neighbor_positive_y = false, is_found_neighbor_negative_y = false;
	bool is_found_neighbor_positive_xy = false, is_found_neighbor_negative_xy = false;
	bool is_found_neighbor_positive_yx = false, is_found_neighbor_negative_yx = false;
	if (terrain_->isTerrainInformation()) {
		// Getting the terrain map
		TerrainDataMap terrain_map = terrain_->getTerrainDataMap();

		double x, y, yaw;

		// Searching the states neighbors
		for (int r = 1; r <= neighboring_definition_; r++) {
			Key searching_key;
			Vertex neighbor_vertex, neighbor_state_vertex;

			// Searching the neighbors in the positive x-axis
			searching_key.x = terrain_key.x + r;
			searching_key.y = terrain_key.y;
			terrain_->getTerrainSpaceModel().keyToVertex(neighbor_vertex, searching_key, true);
			if ((terrain_map.count(neighbor_vertex) > 0) && (!is_found_neighbor_positive_x)) {
				// Getting the state vertex of the neighbor
				terrain_->getTerrainSpaceModel().keyToState(x, searching_key.x, true);
				terrain_->getTerrainSpaceModel().keyToState(y, searching_key.y, true);
				terrain_->getTerrainSpaceModel().keyToState(yaw, key_yaw, false);
				state << x, y, yaw;
				terrain_->getTerrainSpaceModel().stateToVertex(neighbor_state_vertex, state);

				neighbor_states.push_back(neighbor_state_vertex);
				is_found_neighbor_positive_x = true;
			}

			// Searching the neighbor in the negative x-axis
			searching_key.x = terrain_key.x - r;
			searching_key.y = terrain_key.y;
			terrain_->getTerrainSpaceModel().keyToVertex(neighbor_vertex, searching_key, true);
			if ((terrain_map.count(neighbor_vertex) > 0) && (!is_found_neighbor_negative_x)) {
				// Getting the state vertex of the neighbor
				terrain_->getTerrainSpaceModel().keyToState(x, searching_key.x, true);
				terrain_->getTerrainSpaceModel().keyToState(y, searching_key.y, true);
				terrain_->getTerrainSpaceModel().keyToState(yaw, key_yaw, false);
				state << x, y, yaw;
				terrain_->getTerrainSpaceModel().stateToVertex(neighbor_state_vertex, state);

				neighbor_states.push_back(neighbor_state_vertex);
				is_found_neighbor_negative_x = true;
			}

			// Searching the neighbor in the positive y-axis
			searching_key.x = terrain_key.x;
			searching_key.y = terrain_key.y + r;
			terrain_->getTerrainSpaceModel().keyToVertex(neighbor_vertex, searching_key, true);
			if ((terrain_map.count(neighbor_vertex) > 0) && (!is_found_neighbor_positive_y)) {
				// Getting the state vertex of the neighbor
				terrain_->getTerrainSpaceModel().keyToState(x, searching_key.x, true);
				terrain_->getTerrainSpaceModel().keyToState(y, searching_key.y, true);
				terrain_->getTerrainSpaceModel().keyToState(yaw, key_yaw, false);
				state << x, y, yaw;
				terrain_->getTerrainSpaceModel().stateToVertex(neighbor_state_vertex, state);

				neighbor_states.push_back(neighbor_state_vertex);
				is_found_neighbor_positive_y = true;
			}

			// Searching the neighbor in the negative y-axis
			searching_key.x = terrain_key.x;
			searching_key.y = terrain_key.y - r;
			terrain_->getTerrainSpaceModel().keyToVertex(neighbor_vertex, searching_key, true);
			if ((terrain_map.count(neighbor_vertex) > 0) && (!is_found_neighbor_negative_y)) {
				// Getting the state vertex of the neighbor
				terrain_->getTerrainSpaceModel().keyToState(x, searching_key.x, true);
				terrain_->getTerrainSpaceModel().keyToState(y, searching_key.y, true);
				terrain_->getTerrainSpaceModel().keyToState(yaw, key_yaw, false);
				state << x, y, yaw;
				terrain_->getTerrainSpaceModel().stateToVertex(neighbor_state_vertex, state);

				neighbor_states.push_back(neighbor_state_vertex);
				is_found_neighbor_negative_y = true;
			}

			// Searching the neighbor in the positive xy-axis
			searching_key.x = terrain_key.x + r;
			searching_key.y = terrain_key.y + r;
			terrain_->getTerrainSpaceModel().keyToVertex(neighbor_vertex, searching_key, true);
			if ((terrain_map.count(neighbor_vertex) > 0) && (!is_found_neighbor_positive_xy)) {
				// Getting the state vertex of the neighbor
				terrain_->getTerrainSpaceModel().keyToState(x, searching_key.x, true);
				terrain_->getTerrainSpaceModel().keyToState(y, searching_key.y, true);
				terrain_->getTerrainSpaceModel().keyToState(yaw, key_yaw, false);
				state << x, y, yaw;
				terrain_->getTerrainSpaceModel().stateToVertex(neighbor_state_vertex, state);

				neighbor_states.push_back(neighbor_state_vertex);
				is_found_neighbor_positive_xy = true;
			}

			// Searching the neighbor in the negative xy-axis
			searching_key.x = terrain_key.x - r;
			searching_key.y = terrain_key.y - r;
			terrain_->getTerrainSpaceModel().keyToVertex(neighbor_vertex, searching_key, true);
			if ((terrain_map.count(neighbor_vertex) > 0) && (!is_found_neighbor_negative_xy)) {
				// Getting the state vertex of the neighbor
				terrain_->getTerrainSpaceModel().keyToState(x, searching_key.x, true);
				terrain_->getTerrainSpaceModel().keyToState(y, searching_key.y, true);
				terrain_->getTerrainSpaceModel().keyToState(yaw, key_yaw, false);
				state << x, y, yaw;
				terrain_->getTerrainSpaceModel().stateToVertex(neighbor_state_vertex, state);

				neighbor_states.push_back(neighbor_state_vertex);
				is_found_neighbor_negative_xy = true;
			}

			// Searching the neighbor in the positive yx-axis
			searching_key.x = terrain_key.x - r;
			searching_key.y = terrain_key.y + r;
			terrain_->getTerrainSpaceModel().keyToVertex(neighbor_vertex, searching_key, true);
			if ((terrain_map.count(neighbor_vertex) > 0) && (!is_found_neighbor_positive_yx)) {
				// Getting the state vertex of the neighbor
				terrain_->getTerrainSpaceModel().keyToState(x, searching_key.x, true);
				terrain_->getTerrainSpaceModel().keyToState(y, searching_key.y, true);
				terrain_->getTerrainSpaceModel().keyToState(yaw, key_yaw, false);
				state << x, y, yaw;
				terrain_->getTerrainSpaceModel().stateToVertex(neighbor_state_vertex, state);

				neighbor_states.push_back(neighbor_state_vertex);
				is_found_neighbor_positive_yx = true;
			}

			// Searching the neighbor in the negative yx-axis
			searching_key.x = terrain_key.x + r;
			searching_key.y = terrain_key.y - r;
			terrain_->getTerrainSpaceModel().keyToVertex(neighbor_vertex, searching_key, true);
			if ((terrain_map.count(neighbor_vertex) > 0) && (!is_found_neighbor_negative_yx)) {
				// Getting the state vertex of the neighbor
				terrain_->getTerrainSpaceModel().keyToState(x, searching_key.x, true);
				terrain_->getTerrainSpaceModel().keyToState(y, searching_key.y, true);
				terrain_->getTerrainSpaceModel().keyToState(yaw, key_yaw, false);
				state << x, y, yaw;
				terrain_->getTerrainSpaceModel().stateToVertex(neighbor_state_vertex, state);

				neighbor_states.push_back(neighbor_state_vertex);
				is_found_neighbor_negative_yx = true;
			}
		}
	} else
		printf(RED "Could not searched the neighbors because there is not"
				" terrain information \n" COLOR_RESET);
}


void GridBasedBodyAdjacency::computeBodyCost(double& cost,
											 Vertex state_vertex)
{
	// Converting the vertex to state (x,y,yaw)
	Eigen::Vector3d state;
	terrain_->getTerrainSpaceModel().vertexToState(state, state_vertex);

	// Getting the terrain map
	TerrainDataMap terrain_map = terrain_->getTerrainDataMap();

	// Computing the terrain cost
	double terrain_cost = 0;
	unsigned int area_size = stance_areas_.size();
	for (unsigned int n = 0; n < area_size; n++) {
		// Computing the boundary of stance area
		Eigen::Vector2d boundary_min, boundary_max;
		boundary_min(0) = stance_areas_[n].min_x + state(0);
		boundary_min(1) = stance_areas_[n].min_y + state(1);
		boundary_max(0) = stance_areas_[n].max_x + state(0);
		boundary_max(1) = stance_areas_[n].max_y + state(1);

		// Computing the stance cost
		std::set< std::pair<Weight, Vertex>, pair_first_less<Weight, Vertex> > stance_cost_queue;
		double stance_cost = 0;
		double resolution = stance_areas_[n].resolution;
		for (double y = boundary_min(1); y <= boundary_max(1); y += resolution ) {
			for (double x = boundary_min(0); x <= boundary_max(0); x += resolution) {
				// Computing the rotated coordinate according to the orientation of the body
				Eigen::Vector2d point_position;
				point_position(0) = (x - state(0)) * cos((double) state(2)) -
						(y - state(1)) * sin((double) state(2)) + state(0);
				point_position(1) = (x - state(0)) * sin((double) state(2)) +
						(y - state(1)) * cos((double) state(2)) + state(1);

				Vertex current_2d_vertex;
				terrain_->getTerrainSpaceModel().coordToVertex(current_2d_vertex, point_position);

				// Inserts the element in an organized vertex queue, according to the maximum value
				if (terrain_map.count(current_2d_vertex) > 0)
					stance_cost_queue.insert(std::pair<Weight, Vertex>(
							terrain_->getTerrainCost(current_2d_vertex),
							current_2d_vertex));
			}
		}

		// Averaging the 5-best (lowest) cost
		unsigned int number_top_cost = number_top_cost_;
		if (stance_cost_queue.size() < number_top_cost)
			number_top_cost = stance_cost_queue.size();

		if (number_top_cost == 0) {
			stance_cost += uncertainty_factor_ * terrain_->getAverageCostOfTerrain();
		} else {
			for (unsigned int i = 0; i < number_top_cost; i++) {
				stance_cost += stance_cost_queue.begin()->first;
				stance_cost_queue.erase(stance_cost_queue.begin());
			}

			stance_cost /= number_top_cost;
		}

		terrain_cost += stance_cost;
	}
	terrain_cost /= stance_areas_.size();

	// Getting robot and terrain information
	RobotAndTerrain info;
	Eigen::Vector3d default_action;
	default_action << 1, 0, 0;
	info.body_action = default_action;
	info.pose.position = (Eigen::Vector2d) state.head(2);
	info.pose.orientation = (double) state(2);
	info.height_map = terrain_->getTerrainHeightMap();
	info.resolution = terrain_->getResolution(true);

	// Computing the cost of the body features
	cost = terrain_cost;
	unsigned int feature_size = features_.size();
	for (unsigned int i = 0; i < feature_size; i++) {
		// Computing the cost associated with body path features
		double feature_cost, weight;
		features_[i]->computeCost(feature_cost, info);
		features_[i]->getWeight(weight);

		// Computing the cost of the body feature
		cost += weight * feature_cost;
	}
}


bool GridBasedBodyAdjacency::isStanceAdjacency()
{
	return is_stance_adjacency_;
}

} //@namespace model
} //@namespace dwl
