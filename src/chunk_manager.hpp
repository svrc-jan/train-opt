#pragma once

#include <cstdint>
#include <vector>

#include "instance.hpp"
#include "preprocess.hpp"
#include "solver.hpp"

class Solver;

class Chunk_manager
{
public:

	struct Chunk;

	typedef Instance::idx_t idx_t;
	typedef Instance::dur_t dur_t;
	typedef Instance::tim_t tim_t;
	typedef Preprocess::Chunk_state Chunk_state;

	typedef Event_graph::edg_t edg_t; 
	typedef Event_graph::vtx_t vtx_t;

	static constexpr idx_t IDX_MAX = Instance::IDX_MAX;
	static constexpr dur_t DUR_MAX = Instance::DUR_MAX;
	static constexpr tim_t TIM_MAX = Instance::TIM_MAX;

	static constexpr edg_t EDG_MAX = Event_graph::EDG_MAX;
	static constexpr vtx_t VTX_MAX = Event_graph::VTX_MAX;

	const Instance& inst;
	const Preprocess& prepr;

	std::vector<Chunk> chunks = {};
	std::vector<Array<Chunk*>> res = {};

	Chunk_manager(Solver& solver);
	~Chunk_manager();


	void init_data();

	inline void state_change(idx_t c);
	inline void time_change(idx_t l);

	void sync_state();
	void sync_time();
	void sync_res();

private:
	Solver& slvr;

	uint8_t need_state_sync = false;
	uint8_t need_time_sync = false;
	uint8_t need_res_sync = false;
	
	Flag state_dirty;
	Flag time_dirty;
	Flag res_dirty;

	std::vector<Chunk*> res_data = {};
	std::vector<std::set<idx_t>> level_chunks = {};

	void init_levels();
	void init_chunks();
	void init_res();

	bool update_level_chunks(idx_t c, idx_t l_old, idx_t l_new);
};


inline void Chunk_manager::state_change(idx_t c)
{

	this->state_dirty += c;
	this->need_state_sync = true;
}

inline void Chunk_manager::time_change(idx_t l)
{
	auto& lc = this->level_chunks[l];
	if (lc.empty()) {
		return;
	}

	for (auto c : lc) {
		this->time_dirty += c;
	}

	this->need_time_sync = true;
}

struct Chunk_manager::Chunk
{
	idx_t idx = IDX_MAX;
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