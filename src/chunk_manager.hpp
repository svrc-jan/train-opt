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
	struct Link;
	struct Res;
	struct Chain;

	typedef Instance::idx_t idx_t;
	typedef Instance::dur_t dur_t;
	typedef Instance::tim_t tim_t;
	typedef Preprocess::Chunk_state Chunk_state;

	typedef Event_graph::edg_t edg_t; 
	typedef Event_graph::vtx_t vtx_t;
	typedef Event_graph::Ordering Ordering;

	static constexpr idx_t IDX_MAX = Instance::IDX_MAX;
	static constexpr dur_t DUR_MAX = Instance::DUR_MAX;
	static constexpr tim_t TIM_MAX = Instance::TIM_MAX;

	static constexpr edg_t EDG_MAX = Event_graph::EDG_MAX;
	static constexpr vtx_t VTX_MAX = Event_graph::VTX_MAX;

	const Instance& inst;
	const Preprocess& prepr;

	Flag is_active;

	std::vector<Chunk_state> state = {};
	std::vector<Link> link = {};
	std::vector<Interval<tim_t>> time = {};

	std::vector<idx_t> res_idx = {};
	std::vector<idx_t> train_idx = {};

	std::vector<Res> res = {};

	Chunk_manager(Solver& solver);
	~Chunk_manager();

	void init_data();

	inline void state_change(idx_t c);
	inline void time_change(idx_t l);

	void sync_state();
	void sync_time();
	void sync_res();

	void print_links();
	bool print_chunk_link(idx_t c);

	inline bool is_fwd_link(idx_t c_from, idx_t c_to);
	inline bool is_bkwd_link(idx_t c_from, idx_t c_to);
	inline bool is_any_link(idx_t a, idx_t b);


	inline Ordering get_ordering(idx_t c_from, idx_t c_to) const;


private:
	struct Time_cmp;

	Solver& slvr;

	uint8_t need_state_sync = false;
	uint8_t need_time_sync = false;
	uint8_t need_res_sync = false;
	
	Flag state_dirty;
	Flag time_dirty;
	Flag res_dirty;

	std::vector<idx_t> res_data = {};
	std::vector<std::set<idx_t>> level_chunks = {};

	std::set<idx_t> new_link_fwd = {};
	std::set<idx_t> new_link_bkwd = {};

	void init_levels();
	void init_chunks();
	void init_res();

	bool update_level_chunks(idx_t c, idx_t l_old, idx_t l_new);
};


struct Chunk_manager::Link
{
	std::set<idx_t> fwd;
	std::set<idx_t> bkwd;

	size_t size() const { return fwd.size() + bkwd.size(); }
	inline const std::set<idx_t>& get_combination() const; 
};


struct Chunk_manager::Res
{
	idx_t idx = IDX_MAX;
	idx_t size = 0;
	idx_t cap = 0;
	idx_t* chunks = nullptr;
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


inline bool Chunk_manager::is_fwd_link(idx_t c_from, idx_t c_to)
{
	bool ret = this->link[c_from].fwd.contains(c_to);
	assert(!ret || this->link[c_to].bkwd.contains(c_from));

	return ret;
}


inline bool Chunk_manager::is_bkwd_link(idx_t c_from, idx_t c_to)
{
	bool ret = this->link[c_from].bkwd.contains(c_to);
	assert(!ret || this->link[c_to].fwd.contains(c_from));
	
	return ret;
}

inline bool Chunk_manager::is_any_link(idx_t a, idx_t b)
{
	return this->is_fwd_link(a, b) || this->is_bkwd_link(a, b);
};

inline Chunk_manager::Ordering Chunk_manager::get_ordering(idx_t c_from, idx_t c_to) const
{
	auto& state_from = this->state[c_from];
	auto& state_to = this->state[c_to];

	return Ordering(state_from.level.end, state_to.level.start, state_from.rel_time);
}

struct Chunk_manager::Time_cmp
{
	const std::vector<Interval<tim_t>>& time;
	bool operator()(idx_t a, idx_t b) const { return time[a] < time[b]; }
};


inline const std::set<Chunk_manager::idx_t>& Chunk_manager::Link::get_combination() const
{
	static std::set<idx_t> ret;

	ret.clear();
	std::set_union(bkwd.cbegin(), bkwd.cend(), fwd.cbegin(), fwd.cend(), 
		std::inserter(ret, ret.begin()));

	return ret;
}
