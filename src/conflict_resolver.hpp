#pragma once

#include <chrono>

#include "gurobi_c++.h"

#include "utils/tracked.hpp"
#include "instance.hpp"
#include "preprocess.hpp"
#include "link_graph.hpp"
#include "event_graph.hpp"
#include "chunk_manager.hpp"

class Solver;

class Conflict_resolver
{
public:
	enum Opt_state {FAILED, OPTIMAL, CUTOFF};

	struct Conflict;
	struct Obj;
	
	typedef Instance::idx_t idx_t;
	typedef Instance::dur_t dur_t;
	typedef Instance::tim_t tim_t;
	typedef Preprocess::idx_pr idx_pr;

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


	Conflict_resolver(const Preprocess& prepr, Link_graph& link_graph, 
		Chunk_manager& chunk_mngr, GRBEnv& grb_env);

	~Conflict_resolver();

	void solve();
	void set_ops(const Flag& op_active);
	void clear_all();

	tim_t get_obj_val();


private:
	struct Constr;
	struct Cycle_constr;
	struct Path_constr;
	struct Conf_assign;

	Link_graph& link_graph;
	Chunk_manager& chunk_mngr;
	GRBModel model;

	Event_graph event_graph;
	
	std::vector<Conflict> confs = {};
	std::vector<Obj> objs = {};

	std::vector<Event_graph::Vertex_edge> path = {};

	std::vector<Cycle_constr> cycle_constrs = {};
	std::vector<Path_constr> path_constrs = {};

	std::vector<Conf_assign> conf_hlpr = {};
	std::vector<Conf_assign> conf_hlpr2 = {};

	std::vector<Edge> conf_edges;
	std::vector<idx_t> flag_list;

	Flag level_time_change;
	int8_t need_model_update = false;

	std::chrono::steady_clock::time_point solve_start;
	std::chrono::steady_clock::time_point solve_timeout;
	size_t timeout = 600;

	bool resolve_conflicts();

	void add_conflict(idx_pr chunk);
	void make_cycle_cons();
	bool make_path_cons();

	void sync_graph();
	void make_conf_edges(const Conflict& conf, int8_t value);

	void init_levels();

	void init_model();
	int optimize_model();
	void unfreeze_iis();
	void sync_values();

	void remove_cons(Constr& cons);
	void add_model_cycle_cons(Cycle_constr& cons);
	void add_model_path_cons(Path_constr& cons);

	void remove_non_binding();
	

};


struct Conflict_resolver::Conflict
{
	idx_t idx = IDX_MAX;
	int8_t frozen = false;
	Tracked<int8_t> active = false;
	Tracked<int8_t> value = false;

	std::vector<idx_pr> chunks = {};
	GRBVar var;

	GRBLinExpr to_expr(int8_t x) const { return (x ? var : (1 - var)); }

	void freeze();
	void unfreeze();
};


struct Conflict_resolver::Obj
{
	idx_t idx = IDX_MAX;
	idx_t level = IDX_MAX;
	int8_t is_bin = false;
	dur_t coeff = 0;
	tim_t threshold = 0;
	tim_t value = 0;
	GRBVar var;
};


struct Conflict_resolver::Constr
{
	std::vector<Conf_assign> confs = {};
	GRBConstr model;
	int8_t in_model = false;
};


struct Conflict_resolver::Cycle_constr : Conflict_resolver::Constr
{

};


struct Conflict_resolver::Path_constr : Conflict_resolver::Constr
{
	idx_t obj_idx = IDX_MAX;
	tim_t delay = 0;
};


struct Conflict_resolver::Conf_assign
{
	idx_t idx = IDX_MAX;
	int8_t value = 0;

	inline bool operator<(const Conf_assign& x) const { return idx < x.idx; }
	inline bool operator==(const Conf_assign& x) const { return idx == x.idx; }
};
