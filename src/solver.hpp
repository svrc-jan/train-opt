#pragma once

#include <set>

#include "gurobi_c++.h"

#include "preprocess.hpp"
#include "event_graph.hpp"
#include "chunk_manager.hpp"
#include "route_planner.hpp"
#include "conflict_resolver.hpp"


#define GBR_EXCEPTION 20


class Chunk_manager;
class Route_planner;
class Conflict_resolver;


class Solver
{
public:
	struct Ordering;

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
	Event_graph event_graph;
	GRBEnv& grb_env;

	std::unique_ptr<Chunk_manager> chunk_mngr = nullptr;
	std::unique_ptr<Route_planner> route_plnr = nullptr;
	std::unique_ptr<Conflict_resolver> conf_rslvr = nullptr;

	std::vector<idx_t> need_list = {};

	Solver(const Preprocess& prepr, GRBEnv& grb_env);
	~Solver();

	void solve();
	void sync_graph();

	inline void graph_change() { this->need_graph_sync = true; }

	inline Event_graph::tim_t time(vtx_t v) const { return this->event_graph.time[v]; }
	
private:

	uint8_t need_graph_sync = false;
	uint8_t expect_cycle = false;
	
	Flag graph_time_dirty;
	
	void init_data();
	void init_levels();

	idx_t get_most_conflicting_train();
};

struct Solver::Ordering
{
	idx_t from = IDX_MAX;
	idx_t to = IDX_MAX;
	dur_t dur = 0;

	Edge to_edge(vtx_t idx=EDG_MAX) const
	{ return Edge({from, to}, idx, dur); }

	inline bool operator==(const Ordering& x) const
	{ return (from == x.from) && (to == x.to) && (dur == x.dur); }

	inline bool operator!=(const Ordering& x) const
	{ return !(*this == x); }
};

