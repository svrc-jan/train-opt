#pragma once

#include <cstdint>
#include <vector>

#include "utils/unord_pair.hpp"
#include "instance.hpp"
#include "preprocess.hpp"
#include "solver.hpp"

class Solver;

class Chunk_manager
{
public:
	struct Res;

	typedef Instance::idx_t idx_t;
	typedef Instance::dur_t dur_t;
	typedef Instance::tim_t tim_t;
	typedef Preprocess::idx_pr idx_pr;
	typedef Preprocess::State State;

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

	Flag is_active;

	std::vector<State> state = {};
	std::vector<Interval<tim_t>> time = {};
	std::vector<Res> res = {};

	std::vector<idx_t> res_idx = {};
	std::vector<idx_t> train_idx = {};

	

	Chunk_manager(Solver& solver);
	~Chunk_manager();

	void init_data();

	void sync_state(const Flag& op_dirty, const Flag& op_active);
	void sync_time(const Flag& level_time_dirty);
	void sync_res();

	inline Edge get_edge(const idx_pr& chunk, edg_t e) const;


private:
	struct Time_cmp;

	Solver& slvr;
	
	Flag state_dirty;
	Flag time_dirty;
	Flag res_dirty;

	std::vector<idx_t> res_data = {};
	std::vector<std::vector<idx_t>> level_chunks = {};

	void init_levels();
	void init_chunks();
	void init_res();

	bool update_level_chunks(idx_t c, idx_t l_old, idx_t l_new);
};


struct Chunk_manager::Res
{
	idx_t idx = IDX_MAX;
	idx_t size = 0;
	idx_t* chunks = nullptr;
};


struct Chunk_manager::Time_cmp
{
	const std::vector<Interval<tim_t>>& time;
	bool operator()(idx_t a, idx_t b) const { return time[a] < time[b]; }
};



inline Event_graph::Edge Chunk_manager::get_edge(const idx_pr& chunk, edg_t e) const
{
	auto& state_from = this->state[chunk.first];
	auto& state_to = this->state[chunk.second];
	return {{state_from.level.end, state_to.level.start}, state_from.dur, e};
}