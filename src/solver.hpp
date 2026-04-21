#pragma once

#include <set>

#include "gurobi_c++.h"

#include "instance.hpp"
#include "preprocess.hpp"
#include "event_graph.hpp"


enum Solver_state
{
	SLVR_DONE,
	SLVR_FAIL,
	SLVR_OPTIMIZE_MODEL,
	SLVR_UPDATE_VALUES,
	SLRV_UPDATE_GRAPH,
	SLVR_UPDATE_OBJ,
	SLVR_ADD_CONFLICT
};

class Solver
{
public:
	struct Obj;
	struct Route;
	struct Chunk;

	struct Conflict;
	struct Var_assign;
	struct Cycle_cons;
	struct Route_cons;
	struct Path_cons;

	const Instance& inst;
	const Preprocess& prepr;

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

	Solver(const Preprocess& prepr, GRBEnv& grb_env);
	~Solver();

	void solve();

private:
	GRBEnv& grb_env;
	GRBModel model;

	double obj_val = 0;

	enum Solver_state state;

	size_t n_routes = 0;
	std::vector<Obj> objs = {};
	std::vector<Route> routes = {};
	std::vector<Conflict> conflicts = {};

	std::vector<std::vector<idx_t>> res_chunks; 
	std::vector<Chunk> chunks = {};

	std::vector<GRBConstr> route_cons = {};
	std::vector<Cycle_cons> cycle_cons = {};
	std::vector<Path_cons> path_cons = {};

	Event_graph event_graph;

	idx_t curr_train = IDX_MAX;

	std::set<edg_t> assign_set;

	void init_data();
	void init_objs();
	void init_routes();
	void init_chunks();


	void solver_loop();
	
	void optimize_model();

	void update_values();
	
	void update_graph();
	bool update_route_edges();
	bool update_conf_edges();
	void add_cycle_cons();

	void update_objs();
	bool add_obj_cons();

	bool add_conflict();

	void clear_model();
	void freeze_conflicts();

	std::vector<Var_assign> collect_assigns(
		const std::vector<Event_graph::Vertex_edge>& path);
	
};

struct Solver::Obj
{
	tim_t value = 0;
	GRBVar var;
	const Preprocess::Obj* prepr;
};


struct Solver::Route
{
	uint8_t value = 0;
	uint8_t in_graph = 0;
	uint8_t in_chunks = 0;
	uint8_t in_model = 0;
	GRBVar var;

	const Preprocess::Route* prepr;
};

struct Solver::Chunk
{
	Interval<tim_t> tim = {0, TIM_MAX};
	const Preprocess::Chunk* prepr = nullptr;

	inline bool operator<(const Chunk& x) const { return tim < x.tim; }
	inline bool operator==(const Chunk& x) const { return tim == x.tim; }
};

struct Solver::Conflict
{
	idx_t idx = IDX_MAX;
	uint8_t value = 1;
	uint8_t in_model = 0;

	uint8_t graph_update = true;
	Edge graph_edge = Edge();

	GRBVar var;

	std::pair<
		const Preprocess::Chunk*, 
		const Preprocess::Chunk*
	> chunks = {nullptr, nullptr};
};





struct Solver::Var_assign
{
	mutable GRBVar var;
	uint8_t value = 0;

	inline bool operator==(const Var_assign& x) const
	{ return value == x.value && var.sameAs(x.var); }
	
	inline GRBLinExpr to_expr() const
	{ return (value == 1) ? var : (1 - var); } 
};


struct Solver::Cycle_cons
{
	uint8_t in_model = false;
	std::vector<Var_assign> assigns = {};
	GRBConstr model_cons;
	
	void add_to_model(GRBModel& model);
	void remove_from_model(GRBModel& model);
};


struct Solver::Path_cons
{
	uint8_t in_model = false;
	tim_t delay = 0;
	const Obj* obj = nullptr;
	std::vector<Var_assign> assigns = {};
	GRBConstr model_cons;

	void add_to_model(GRBModel& model);
	void remove_from_model(GRBModel& model);
};
