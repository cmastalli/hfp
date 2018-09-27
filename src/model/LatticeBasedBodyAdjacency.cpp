#include <dwl/model/LatticeBasedBodyAdjacency.h>


namespace dwl
{

namespace model
{

LatticeBasedBodyAdjacency::LatticeBasedBodyAdjacency() : robot_(NULL),
		terrain_(NULL), is_stance_adjacency_(true),	number_top_cost_(10),
		uncertainty_factor_(1.15)
{
	name_ = "Lattice-based Body";
	is_lattice_ = true;
}


LatticeBasedBodyAdjacency::~LatticeBasedBodyAdjacency()
{

}


void LatticeBasedBodyAdjacency::reset(robot::Robot* robot,
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


void LatticeBasedBodyAdjacency::getSuccessors(std::list<Edge>& successors,
											  Vertex state_vertex)
{
	// Getting the 3d pose for generating the actions
	std::vector<Action3d> actions;
	Eigen::Vector3d current_state;
	terrain_->getTerrainSpaceModel().vertexToState(current_state, state_vertex);

	// Converting state to pose
	Pose3d current_pose;
	current_pose.position = current_state.head(2);
	current_pose.orientation = current_state(2);

	// Gets actions according the defined body motor primitives
	robot_->getBodyMotorPrimitive().generateActions(actions, current_pose);

	// Evaluating every action (body motor primitives)
	if (terrain_->isTerrainInformation()) {
		unsigned int action_size = actions.size();
		for (unsigned int i = 0; i < action_size; i++) {
			// Converting the action to current vertex
			Eigen::Vector3d action_state;
			action_state << actions[i].pose.position, actions[i].pose.orientation;
			Vertex current_action_vertex, terrain_vertex;
			terrain_->getTerrainSpaceModel().stateToVertex(current_action_vertex, action_state);

			// Converting state vertex to environment vertex
			terrain_->getTerrainSpaceModel().stateVertexToEnvironmentVertex(terrain_vertex,
					current_action_vertex, XY_Y);

			// Computing the current action
			current_action_ = action_state - current_state;

			// Checks if there is an obstacle
			if (isFreeOfObstacle(current_action_vertex, XY_Y, true)) {
				if (!isStanceAdjacency()) {
					double terrain_cost;
					if (terrain_->getTerrainDataMap().count(terrain_vertex) == 0)
						terrain_cost = uncertainty_factor_ * terrain_->getAverageCostOfTerrain();
					else
						terrain_cost = terrain_->getTerrainCost(terrain_vertex);

					successors.push_back(Edge(current_action_vertex, terrain_cost));
				} else {
					// Computing the body cost
					double body_cost;
					computeBodyCost(body_cost, action_state);
					body_cost += actions[i].cost;
					successors.push_back(Edge(current_action_vertex, body_cost));
				}
			}
		}
	} else
		printf(RED "Could not computed the successors because there is not terrain information \n"
				COLOR_RESET);
}


void LatticeBasedBodyAdjacency::computeBodyCost(double& cost,
												Eigen::Vector3d state)
{
	// Getting the stance areas according to the action
	stance_areas_ = robot_->getFootstepSearchAreas(current_action_);

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
		for (double y = boundary_min(1); y <= boundary_max(1); y += resolution) {
			for (double x = boundary_min(0); x <= boundary_max(0); x += resolution) {
				// Computing the rotated coordinate according to the orientation of the body
				Eigen::Vector2d point_position;
				double current_x = state(0);
				double current_y = state(1);
				double current_yaw = state(2);
				point_position(0) = (x - current_x) * cos(current_yaw) -
						(y - current_y) * sin(current_yaw) + current_x;
				point_position(1) = (x - current_x) * sin(current_yaw) +
						(y - current_y) * cos(current_yaw) + current_y;

				Vertex current_2d_vertex;
				terrain_->getTerrainSpaceModel().coordToVertex(current_2d_vertex, point_position);

				// Inserts the element in an organized vertex queue, according to the maximum value
				if (terrain_->getTerrainDataMap().count(current_2d_vertex) > 0) {
					stance_cost_queue.insert(std::pair<Weight, Vertex>(
							terrain_->getTerrainCost(current_2d_vertex),
							current_2d_vertex));
				}
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
	info.body_action = current_action_;
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


bool LatticeBasedBodyAdjacency::isFreeOfObstacle(Vertex state_vertex,
												 TypeOfState state_representation,
												 bool body)
{
	// Getting the terrain obstacle map
	ObstacleMap obstacle_map = terrain_->getObstacleMap();

	// Converting the vertex to state (x,y,yaw)
	Eigen::Vector3d state_3d;
	Eigen::Vector2d state_2d;
	double current_x, current_y, current_yaw;
	if (state_representation == XY) {
		terrain_->getObstacleSpaceModel().vertexToState(state_2d, state_vertex);
		current_x = state_2d(0);
		current_y = state_2d(1);
		current_yaw = 0;
	} else {
		terrain_->getObstacleSpaceModel().vertexToState(state_3d, state_vertex);
		current_x = state_3d(0);
		current_y = state_3d(1);
		current_yaw = state_3d(2);
	}

	bool is_free = true;
	if (terrain_->isObstacleInformation()) {
		if (body) {
			// Getting the body area of the robot
			SearchArea body_workspace = robot_->getPredefinedBodyWorkspace();

			// Computing the boundary of stance area
			Eigen::Vector2d boundary_min, boundary_max;
			boundary_min(0) = body_workspace.min_x + current_x;
			boundary_min(1) = body_workspace.min_y + current_y;
			boundary_max(0) = body_workspace.max_x + current_x;
			boundary_max(1) = body_workspace.max_y + current_y;

			// Getting the resolution of the obstacle map
			double obstacle_resolution = terrain_->getObstacleResolution();
			if (body_workspace.resolution > obstacle_resolution)
				obstacle_resolution = body_workspace.resolution;

			for (double y = boundary_min(1); y <= boundary_max(1); y += obstacle_resolution) {
				for (double x = boundary_min(0); x <= boundary_max(0); x += obstacle_resolution) {
					// Computing the rotated coordinate according to the orientation of the body
					Eigen::Vector2d point_position;
					point_position(0) = (x - current_x) * cos(current_yaw) -
							(y - current_y) * sin(current_yaw) + current_x;
					point_position(1) = (x - current_x) * sin(current_yaw) +
							(y - current_y) * cos(current_yaw) + current_y;

					Vertex current_2d_vertex;
					terrain_->getObstacleSpaceModel().coordToVertex(current_2d_vertex, point_position);

					// Checking if there is an obstacle
					if (obstacle_map.count(current_2d_vertex) > 0) {
						if (obstacle_map.find(current_2d_vertex)->second) {
							is_free = false;
							goto found_obstacle;
						}
					}
				}
			}
		} else {
			// Converting the state vertex to terrain vertex
			Vertex terrain_vertex;
			terrain_->getObstacleSpaceModel().stateVertexToEnvironmentVertex(terrain_vertex,
					state_vertex, state_representation);

			if (obstacle_map.count(terrain_vertex) > 0) {
				if (obstacle_map.find(terrain_vertex)->second)
					is_free = false;
			}
		}
	}

	found_obstacle:
	return is_free;
}


bool LatticeBasedBodyAdjacency::isStanceAdjacency()
{
	return is_stance_adjacency_;
}

} //@namespace model
} //@namespace dwl
