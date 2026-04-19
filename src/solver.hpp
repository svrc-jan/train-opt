#pragma once

#include <set>

#include "gurobi_c++.h"


#include "instance.hpp"
#include "preprocess.hpp"
#include "event_graph.hpp"


enum State
{
	STATE_DONE,
	STATE_SOLVE_MODEL,
	STATE_UPDATE_GRAPH,
	STATE_UPDATE_OBJ,
	STATE_FIND_CONFLICT
};


class Solver
{
public:
	struct Route;
	struct Conflict;
	struct Var_assign;
	struct Cycle_cons;
	struct Path_cons;

	const Instance& inst;
	const Preprocess& prepr;

	typedef Instance::idx_t idx_t;
	typedef Instance::dur_t dur_t;
	typedef Instance::tim_t tim_t;

	static constexpr idx_t IDX_MAX = Instance::IDX_MAX;
	static constexpr dur_t DUR_MAX = Instance::DUR_MAX;
	static constexpr tim_t TIM_MAX = Instance::TIM_MAX;

	Solver(const Preprocess& prepr, GRBEnv& grb_env);
	~Solver();

	void solve();

private:
	GRBEnv& grb_env;
	GRBModel model = nullptr;

	double last_obj_val = 0;

	std::vector<GRBVar> obj_vars = {};
	std::vector<tim_t> obj_values = {};

	std::vector<GRBVar> conf_vars = {};
	std::vector<uint8_t> conf_values = {};

	std::vector<GRBVar> route_vars = {};
	std::vector<uint8_t> route_values = {};
	std::vector<idx_t> route_var_idx = {};
	
	std::vector<Cycle_cons> cycle_cons = {};
	std::vector<Path_cons> path_cons = {};
	std::vector<GRBConstr> route_cons = {};

	Event_graph event_graph;

	std::vector<dur_t> level_dur = {};

	std::vector<std::vector<idx_t>> train_routes = {};

	std::vector<Res_use*> res_use = {};
	std::vector<Res_use> res_use_data = {};
	std::vector<std::vector<Res_interval>> res_ints = {};

	std::vector<Conflict> conflicts;
	std::set<idx_t> assign_set;

	void init_res_use();


	void add_train(idx_t t);
	void add_train_route(idx_t t);
	void add_train_req_ops(idx_t t);
	void add_train_level_dur(idx_t t);
	void add_train_req_objs(idx_t t);
	void add_obj(idx_t train, idx_t level, idx_t inst_idx);

	void clear_model();
	void solve_model();
	bool update_values();

	void update_graph();
	void add_dur_edges();
	void add_conf_edges();

	std::vector<Var_assign> collect_assigns(const std::vector<Event_graph::Edge_vertex>& path);
	void add_cycle_cons();
	bool add_obj_cons();
	bool add_conflict();
	void freeze_conflicts();
};

struct Solver::Obj
{
	idx_t idx = IDX_MAX;
	idx_t train = IDX_MAX;
	idx_t level = IDX_MAX;
	uint8_t is_bin = 0;
	tim_t threshold = 0;
};


struct Solver::Res_use
{
	Interval<idx_t> level = {IDX_MAX, IDX_MAX};
	idx_t train = IDX_MAX;
	dur_t time = 0;
};


struct Solver::Res_interval
{
	idx_t train;
	idx_t res;
	Interval<tim_t> tim = {0, TIM_MAX};

	inline bool operator<(const Res_interval& x) const { return tim < x.tim; }
	inline bool operator==(const Res_interval& x) const { return tim == x.tim; }
};

struct Solver::Conflict
{
	idx_t var = IDX_MAX;
	idx_t res = IDX_MAX;
	uint8_t freeze = 0;
	std::pair<idx_t, idx_t> train = {IDX_MAX, IDX_MAX};
};





struct Solver::Var_assign
{
	idx_t idx = IDX_MAX;
	uint8_t value = 0;

	inline bool operator==(idx_t x) const { return idx == x; }
	inline GRBLinExpr to_expr(const std::vector<GRBVar>& vars);
};


struct Solver::Cycle_cons
{
	uint8_t in_model = 0;
	GRBConstr model_cons;
	std::vector<Var_assign> assigns = {};
	
	void add_to_model(GRBModel& model, const std::vector<GRBVar>& conf_vars);
	void remove_from_model(GRBModel& model);
};


struct Solver::Path_cons
{
	uint8_t in_model = 0;
	uint8_t is_bin = 0;
	idx_t obj_idx = IDX_MAX;
	tim_t delay = 0;
	GRBConstr model_cons;
	std::vector<Var_assign> assigns = {};

	void add_to_model(GRBModel& model, const std::vector<GRBVar>& conf_vars,
		const std::vector<GRBVar>& obj_vars);
	void remove_from_model(GRBModel& model);
};

inline GRBLinExpr Solver::Var_assign::to_expr(const std::vector<GRBVar>& vars)
{
	auto& var = vars[idx];
	return (value == 1) ? var : (1 - var); 
}


inline std::ostream& operator<<(std::ostream& os, const Solver::Var_assign& x)
{
	os << "v" << x.idx << ":" << (uint64_t)x.value;
	return os;
}