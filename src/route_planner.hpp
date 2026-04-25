#pragma once

#include "gurobi_c++.h"

#include "utils/tracked.hpp"
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
	typedef Event_graph::Ordering Ordering;

	static constexpr idx_t IDX_MAX = Instance::IDX_MAX;
	static constexpr dur_t DUR_MAX = Instance::DUR_MAX;
	static constexpr tim_t TIM_MAX = Instance::TIM_MAX;

	static constexpr edg_t EDG_MAX = Event_graph::EDG_MAX;
	static constexpr vtx_t VTX_MAX = Event_graph::VTX_MAX;

	const Instance& inst;
	const Preprocess& prepr;

	Route_planner(Solver& sovler);
	~Route_planner();

	void init_data();
	
	void sync_route_to_op();
	void sync_op_to_route();

	void sync_event_graph();
	void make_init_routes(); 

private:
	struct Flow_cons;
	
	Solver& slvr;
	GRBModel model;

	double init_dur_stretch = 0.5;

	std::vector<Op> ops = {};
	std::vector<Route> routes = {};

	std::vector<GRBConstr> flow_constr = {};

	std::set<idx_t> assign_random_route_set = {};

	void plan_section_range(const Interval<idx_t>& section_ivl);

	void make_plan_routes(const Interval<idx_t>& section_ivl);
	void make_plan_chunks();

	void init_ops();
	void init_routes();
	void init_model();

	void find_req_routes();
	void add_route_vars();
	void add_flow_constr();

	bool optimize_model();

	void freeze_all();
	void unfreeze_all();

	void freeze_section(const Preprocess::Section& sect);
	void unfreeze_section(const Preprocess::Section& sect);

	void assign_all_random_sections();
	void assign_random_section(const Preprocess::Section& sect);

	void assign_all_sections_dur(double stretch=1.0);
	void assign_section_dur(const Preprocess::Section& sect, double stretch=0.0);
	void assign_op_dur(Op& op, dur_t new_dur);
};


struct Route_planner::Op
{
	Tracked<int8_t> value = {0};
	Tracked<idx_t> succ = {IDX_MAX};

	uint8_t in_graph = 0;
	Tracked<tim_t> dur = {0};
	const Preprocess::Op* prepr = nullptr;
};


struct Route_planner::Route
{
	Tracked<int8_t> value = {0};
	int8_t is_req = 0;
	int8_t is_frozen = 0;
	GRBVar var;
	const Preprocess::Route* prepr = nullptr;

	GRBLinExpr to_expr() const { return (is_req ? 1 : GRBLinExpr(var)); }
	void freeze();
	void unfreeze();
};