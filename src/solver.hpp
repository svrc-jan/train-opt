#pragma once

#include <set>

#include "gurobi_c++.h"

#include "utils/batch.hpp"
#include "instance.hpp"
#include "preprocess.hpp"
#include "link_graph.hpp"
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

	Link_graph link_graph;
	Event_graph event_graph;

	GRBEnv& grb_env;

	std::unique_ptr<Chunk_manager> chunk_mngr = nullptr;
	std::unique_ptr<Route_planner> route_plnr = nullptr;
	std::unique_ptr<Conflict_resolver> conf_rslvr = nullptr;

	std::vector<idx_t> list_hlpr = {};

	Flag time_dirty;

	Solver(const Preprocess& prepr, GRBEnv& grb_env);
	~Solver();

	void plan_routes();
	void solve();
	void resolve_conflicts(idx_t t);

	inline Event_graph::tim_t time(vtx_t v) const { return this->event_graph.time[v]; }
	
private:
	Batch<idx_t, int8_t> op_changes;
	Batch<idx_pr, int8_t> op_succ_changes;

	void init_data();
	void init_ops();
	void init_levels();

	void get_op_changes();
	void sync_chunk_mngr_state();
	void sync_link_graph();
	bool sync_event_graph();

};
