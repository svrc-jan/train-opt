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
	struct Level;
	struct Chunk;

	struct Flow_cons;

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

	double init_dur_stretch = 0.5;
	double res_dur_stretch = 0.3;

	std::vector<Op> ops = {};
	std::vector<Route> routes = {};
	std::vector<Level> levels = {};
	std::vector<Chunk> chunks = {};

	std::vector<GRBConstr> flow_constr = {};

	Flag is_op_active;
	Flag is_route_active;

	uint8_t need_route_op_sync = false;
	uint8_t need_op_graph_sync = false;

	Flag op_graph_dirty;
	Flag route_op_dirty;

	std::vector<GRBVar> plan_vars = {};
	std::vector<GRBConstr> plan_constrs = {};

	std::set<idx_t> plan_chunk_set = {};
	std::set<idx_t> assign_random_route_set = {};

	void plan_section_range(const Interval<idx_t>& section_ivl);
	
	void make_levels(const Interval<idx_t>& level_ivl);
	void make_level_bounds(const Interval<idx_t>& level_ivl);
	void propagate_level_lbs(const Interval<idx_t>& level_ivl);
	void propagate_level_ubs(const Interval<idx_t>& level_ivl);

	void make_plan_routes(const Interval<idx_t>& section_ivl);
	void make_plan_chunks();

	void init_data();
	void init_ops();
	void init_routes();
	void init_levels();
	void init_chunks();

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
	uint8_t is_req = 0;
	uint8_t is_frozen = 0;
	GRBVar var;
	const Preprocess::Route* prepr = nullptr;

	GRBLinExpr to_expr() const { return (is_req ? 1 : GRBLinExpr(var)); }
	void freeze();
	void unfreeze();
};


struct Route_planner::Level
{
	uint8_t is_fixed = false;
	uint8_t in_model = false;
	tim_t lb = 0;
	tim_t ub = TIM_MAX;
	GRBVar var;
	const Preprocess::Level* prepr = nullptr;
	GRBLinExpr to_expr() const { return (is_fixed ? lb : GRBLinExpr(var)); }
};


struct Route_planner::Chunk
{
	Interval<tim_t> ub = {0, 0};
	Interval<tim_t> lb = {TIM_MAX, TIM_MAX};
	Interval<GRBVar> var;
	const Preprocess::Chunk* prepr = nullptr;
};
