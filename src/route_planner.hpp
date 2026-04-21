#pragma once

#include "gurobi_c++.h"

#include "instance.hpp"
#include "preprocess.hpp"
#include "solver.hpp"

class Solver;

class Route_planner
{
public:
	struct Op;
	struct Route;

	typedef Instance::idx_t idx_t;
	typedef Instance::dur_t dur_t;
	typedef Instance::tim_t tim_t;

	typedef Event_graph::edg_t edg_t; 
	typedef Event_graph::vtx_t vtx_t; 
	typedef Event_graph::Edge Edge;

	static constexpr idx_t IDX_MAX = Instance::IDX_MAX;
	static constexpr dur_t DUR_MAX = Instance::DUR_MAX;
	static constexpr tim_t TIM_MAX = Instance::TIM_MAX;

	static constexpr edg_t EDG_MAX = Event_graph::EDG_MAX;
	static constexpr vtx_t VTX_MAX = Event_graph::VTX_MAX;

	const Instance& inst;
	const Preprocess& prepr;

	Route_planner(Solver& sovler);
	~Route_planner();


	friend class Solver;

private:
	Solver& slvr;
	GRBModel model;

	std::vector<Op> ops = {};
	std::vector<Route> routes = {};

	uint8_t need_route_op_sync = false;
	uint8_t need_op_graph_sync = false;

	Flag is_op_active;
	Flag op_graph_dirty;
	Flag route_op_dirty;

	std::set<idx_t> route_set = {};

	void init_data();

	void update_route(Route& route, bool value);
	inline void update_route(idx_t r, bool value)
	{ this->update_route(this->routes[r], value); };

	void assign_all_random_sections();
	void assign_random_section(const Preprocess::Section& sect);


	void sync_route_ops();
	void sync_op_graph();
};


struct Route_planner::Op
{
	uint8_t active = 0;
	tim_t dur = 0;
	Edge curr_edge = Edge();
	const Preprocess::Op* prepr = nullptr;

	Edge to_edge() const { return (active ? Edge(prepr->level, dur, EDG_MAX) : Edge()); }
};


struct Route_planner::Route
{
	uint8_t value = 0;
	uint8_t active = 0;
	GRBVar var;
	const Preprocess::Route* prepr = nullptr;
};