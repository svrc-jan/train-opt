#pragma once

#include "utils/lex_comp.hpp"
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
	
	Conflict_resolver(Solver& solver);
	~Conflict_resolver();

	void init_data();
	void sync_graph();

	void add_cycle_cons();

	bool add_conflict(idx_t train);

	inline void chunk_state_change(idx_t c);
	inline void chunk_link_change(idx_t c);
	inline void time_change(idx_t l);

	void freeze_conflicts();
	void clear_constrs();

private:
	struct Link;
	struct Constr;
	struct Cycle_constr;
	struct Path_constr;

	Solver& slvr;
	GRBModel model;

	tim_t total_obj = 0;

	std::vector<Conflict> confs = {};
	std::vector<Obj> objs = {};

	std::vector<std::vector<idx_t>> chunk_conf = {};
	std::vector<std::vector<idx_t>> train_conf = {};

	std::map<idx_t, idx_t> level_obj = {};
	

	uint8_t need_state_sync = false;
	uint8_t need_link_sync = false;
	uint8_t need_graph_sync = false;
	uint8_t need_conf_purge = false;
	uint8_t need_value_sync = false;
	uint8_t need_model_sync = false;
	uint8_t need_obj_sync = false;

	std::set<idx_t> conf_state_dirty = {};
	std::set<idx_t> conf_link_dirty = {};
	std::set<idx_t> conf_graph_dirty = {};
	std::set<idx_t> conf_purge = {};

	std::vector<Event_graph::Vertex_edge> path = {};

	std::vector<Cycle_constr> cycle_constrs = {};
	std::vector<Path_constr> path_constrs = {};

	std::set<std::pair<idx_t, idx_t>> chunk_pairs = {};


	void init_chunks();
	void init_objs();
	void init_trains();

	void sync_state();
	void sync_links();
	void sync_model();
	void sync_values();
	void sync_obj();

	void purge_conf();
	bool add_path_cons();

	bool optimize_model();
	void unfreeze_iis();
};


struct Conflict_resolver::Conflict
{
	idx_t idx = IDX_MAX;
	uint8_t active = false;
	uint8_t value = false;
	uint8_t frozen = false;
	std::pair<idx_t, idx_t> chunk = {IDX_MAX, IDX_MAX};
	std::pair<Ordering, Ordering> ordering;
	Edge edge = Edge();
	GRBVar var;
	std::set<Link> links;

	Edge to_edge() const;
	GRBLinExpr to_expr(uint8_t x) const { return (x ? var : (1 - var)); }
	GRBLinExpr to_expr() const { return this->to_expr(this->value); }

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


struct Conflict_resolver::Link
{
	typedef std::pair<idx_t, idx_t> Idx;
	Idx idx = {IDX_MAX, IDX_MAX};
	GRBConstr cons;

	Link(idx_t a, idx_t b) : idx((a < b) ? Idx(a, b) : Idx(b, a)) { assert(a != b); }

	inline bool operator<(const std::pair<idx_t, idx_t>& x) const { return (idx < x); }
	inline bool operator==(const std::pair<idx_t, idx_t>& x) const { return (idx == x); }

	inline bool operator<(const Link& x) const { return (idx < x.idx); }
	inline bool operator==(const Link& x) const { return (idx == x.idx); }
};


struct Conflict_resolver::Constr
{
	std::set<idx_t> conf_set;
	std::vector<uint8_t> conf_values;
	GRBConstr model;

	bool is_conf_overlap(const std::set<idx_t>& x);
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


inline void Conflict_resolver::chunk_state_change(idx_t c)
{
	for (auto k : this->chunk_conf[c]) {
		this->conf_state_dirty.insert(k);
		this->need_state_sync = true;
	}
}


inline void Conflict_resolver::chunk_link_change(idx_t c)
{
	for (auto k : this->chunk_conf[c]) {
		this->conf_link_dirty.insert(k);
		this->need_link_sync = true;
	}
}


inline void Conflict_resolver::time_change(idx_t l)
{
	auto it = this->level_obj.find(l);
	if (it == this->level_obj.end()) {
		return;
	}
	this->need_obj_sync = true;
}