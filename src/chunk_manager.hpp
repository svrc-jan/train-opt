#pragma once

#include <cstdint>
#include <vector>

#include "instance.hpp"
#include "preprocess.hpp"

class Chunk_manager
{
public:
	struct Res;

	typedef Instance::idx_t idx_t;
	typedef Instance::dur_t dur_t;
	typedef Instance::tim_t tim_t;
	typedef Preprocess::idx_pr idx_pr;
	typedef Preprocess::State State;

	static constexpr idx_t IDX_MAX = Instance::IDX_MAX;
	static constexpr dur_t DUR_MAX = Instance::DUR_MAX;
	static constexpr tim_t TIM_MAX = Instance::TIM_MAX;

	const Instance& inst;
	const Preprocess& prepr;

	Flag is_active;

	std::vector<State> state = {};
	std::vector<Interval<tim_t>> time = {};
	std::vector<Res> res = {};

	std::vector<idx_t> res_idx = {};
	std::vector<idx_t> train_idx = {};
	

	Chunk_manager(const Preprocess& prepr);
	~Chunk_manager();

	void op_change(const Flag& op_change);
	void time_change(const std::vector<std::pair<idx_t, tim_t>>& level_time_change);

	void sync_state(const Flag& op_active);
	void sync_time();

	void get_all_conflicts(std::vector<idx_pr>& confs, double stretch=0.0);

private:
	struct Time_cmp;

	Flag state_dirty;
	Flag time_dirty;
	Flag res_dirty;

	std::vector<idx_t> res_data = {};
	std::vector<tim_t> level_time = {};
	std::vector<std::vector<idx_t>> level_chunks = {};

	std::vector<idx_t> flag_list;

	void init_data();
	void init_ops();
	void init_levels();
	void init_chunks();
	void init_res();

	void sync_res();
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
