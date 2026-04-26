#pragma once

#include "utils/unord_pair.hpp"
#include "solver.hpp"

class Solver;

class Conflict_resolver
{
public:
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

	std::map<idx_t, idx_t> crit_path_count;
	
	Conflict_resolver(Solver& solver);
	~Conflict_resolver();

	void init_data();

	void sync_graph();
	int optimize_model();

	idx_pr find_conflict_train(idx_t train);
	idx_pr find_conflict_chunk(idx_t chunk);

	void make_crit_path_count();

	void add_conflict(idx_pr chunk);
	void add_cycle_cons();
	bool add_path_cons();

	void freeze_conflicts();
	void clear_constrs(bool keep_critical=true);
	void unfreeze_train_confs(idx_t train);
	
	tim_t get_obj_val();
	void set_obj_ub(tim_t bound);


private:
	struct Constr;
	struct Cycle_constr;
	struct Path_constr;
	struct Conf_assign;

	Solver& slvr;
	GRBModel model;

	std::vector<Conflict> confs = {};
	std::vector<Obj> objs = {};

	std::vector<idx_t> chunk_conf = {};
	std::vector<std::vector<idx_t>> train_conf = {};

	std::vector<Event_graph::Vertex_edge> path = {};

	std::vector<Cycle_constr> cycle_constrs = {};
	std::vector<Path_constr> path_constrs = {};

	std::vector<Conf_assign> conf_hlpr = {};
	std::vector<Conf_assign> conf_hlpr2 = {};
	
	std::set<idx_pr> conf_chain;
	std::vector<Edge> conf_edges;


	void init_chunks();
	void init_objs();
	void init_trains();

	void unfreeze_iis();
	void sync_values();

	void purge_dominated_cycle(const Cycle_constr& cons);
	void purge_dominated_path(const Path_constr& cons);

	void make_conf_edges(const Conflict& conf, int8_t);

	bool is_cons_conf_active(const Constr& cons);

};


struct Conflict_resolver::Conflict
{
	idx_t idx = IDX_MAX;
	int8_t frozen = false;
	idx_pr train = {IDX_MAX, IDX_MAX};
	Tracked<int8_t> active = false;
	Tracked<int8_t> value = false;

	std::vector<idx_pr> chunks = {};
	GRBVar var;

	GRBLinExpr to_expr(int8_t x) const { return (x ? var : (1 - var)); }
	GRBLinExpr to_expr() const { return this->to_expr(this->value.curr); }

	void freeze();
	void unfreeze();
};


struct Conflict_resolver::Obj
{
	idx_t idx = IDX_MAX;
	uint8_t active = true;
	tim_t value = 0;
	const Preprocess::Obj* prepr = nullptr;
	GRBVar var;
};


struct Conflict_resolver::Constr
{
	std::vector<Conf_assign> confs = {};
	GRBConstr model;

	bool has_conf_subset(const std::vector<Conf_assign>& subset) const;
};


struct Conflict_resolver::Cycle_constr : Conflict_resolver::Constr
{

};


struct Conflict_resolver::Path_constr : Conflict_resolver::Constr
{
	idx_t obj_idx = IDX_MAX;
	uint8_t is_bin = false;
	tim_t delay = 0;
};


struct Conflict_resolver::Conf_assign
{
	idx_t idx = IDX_MAX;
	int8_t value = 0;

	inline bool operator<(const Conf_assign& x) const { return idx < x.idx; }
	inline bool operator==(const Conf_assign& x) const { return idx == x.idx; }
};
