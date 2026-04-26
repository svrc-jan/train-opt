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
	struct Level;
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

	Tracked<Flag> op_active;
	Flag op_dirty;

	std::vector<Tracked<idx_t>> op_succ = {};
	std::vector<Level> levels = {};


	Route_planner(Solver& sovler);
	~Route_planner();

	void init_data();
	void sync_graph();

	void get_random_ops(double dur_stretch=0.0);
	void snap_ops();

private:
	struct Flow_cons;
	
	Solver& slvr;
	GRBModel model;

	double init_dur_stretch = 0.5;

	std::vector<Route> routes = {};
	std::vector<GRBConstr> flow_constr = {};
	std::set<idx_t> assign_random_route_set = {};

	std::vector<double> chunk_price = {};

	void init_ops();
	void init_levels();
	void init_routes();
	void init_model();

	void find_req_routes();
	void add_route_vars();
	void add_flow_constr();

	bool optimize_model();

	void freeze_all();
	void unfreeze_all();

	void freeze_train(idx_t t);
	void unfreeze_train(idx_t t);
};


struct Route_planner::Op
{
	Tracked<idx_t> succ = {IDX_MAX};
	const Preprocess::Op* prepr = nullptr;
};



struct Route_planner::Level
{
	idx_t idx = IDX_MAX;
	Tracked<idx_t> next = {IDX_MAX};
	Tracked<dur_t> dur = {0};
	Tracked<tim_t> lb = {IDX_MAX};

	void stretch_dur(double by) { dur = (dur_t)MIN((double)DUR_MAX, round(dur*(1 + by))); }

	Edge edge_old() const { return {{idx, next.old}, dur.old, EDG_MAX}; }
	Edge edge_curr() const { return {{idx, next.curr}, dur.curr, EDG_MAX}; }
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