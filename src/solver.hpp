#pragma once

#include <set>

#include "gurobi_c++.h"

#include "instance.hpp"
#include "preprocess.hpp"
#include "event_graph.hpp"
#include "conflict_resolver.hpp"
#include "route_planner.hpp"


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

class Route_planner;
class Conflict_resolver;


class Solver
{
public:
	struct Route;
	struct Chunk;

	const Instance& inst;
	const Preprocess& prepr;

	typedef Instance::idx_t idx_t;
	typedef Instance::dur_t dur_t;
	typedef Instance::tim_t tim_t;
	typedef Preprocess::Chunk_state Chunk_state;

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

	friend class Conflict_resolver;
	friend class Route_planner;

private:
	GRBEnv& grb_env;
	Event_graph event_graph;

	std::unique_ptr<Route_planner> route_plnr = nullptr;
	std::unique_ptr<Conflict_resolver> conf_rslvr = nullptr;

	std::vector<Chunk> chunks = {};
	std::vector<std::set<idx_t>> level_chunks = {};

	uint8_t need_level_time_sync = false;
	uint8_t need_chunk_state_sync = false;
	uint8_t need_chunk_time_sync = false;
	uint8_t need_res_chunks_sync = false;
	
	Flag level_time_dirty;
	Flag chunk_state_dirty;
	Flag chunk_time_dirty;
	Flag res_chunks_dirty;

	std::vector<Array<Chunk*>> res_chunks = {};
	std::vector<Chunk*> res_chunks_data = {};

	std::vector<idx_t> need_list = {};

	void init_data();
	void init_chunks();
	void init_res_chunks();

	void sync_level_times();
	void sync_chunk_state();
	void sync_chunk_times();
	void sync_res_chunks();
	
	void update_level_chunks(idx_t c, idx_t l_old, idx_t l_new);
};


struct Solver::Chunk
{
	Chunk_state state;
	Interval<tim_t> time = {0, TIM_MAX};
	const Preprocess::Chunk* prepr = nullptr;
	std::vector<idx_t> conflicts = {};

	inline bool operator<(const Chunk& x) const { return this->time < x.time; }
	// inline bool operator==(const Chunk& x) const { return this->time == x.time; }
	
	inline bool is_active() const 
	{ return (state.level.start < IDX_MAX) && (state.level.end < IDX_MAX); }

	inline bool has_active_time() const
	{ return (time.start < TIM_MAX) && (time.end < TIM_MAX); }


	static bool ptr_cmp(const Chunk* const a, const Chunk* const b) { return a->time < b->time; }
};


