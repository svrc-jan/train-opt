#include "chunk_manager.hpp"

#include <iostream>
#include <algorithm>

#include "utils/stl_print.hpp"

using namespace std;


Chunk_manager::Chunk_manager(const Preprocess& prepr)
	: inst(prepr.inst), prepr(prepr)
{
	this->init_data();
}


Chunk_manager::~Chunk_manager()
{

}


void Chunk_manager::init_data()
{
	this->init_ops();
	this->init_levels();
	this->init_chunks();
	this->init_res();
}


void Chunk_manager::init_ops()
{
	// this->op_active.set_n_items(this->inst.n_ops());
}


void Chunk_manager::init_levels()
{
	this->level_chunks.resize(this->prepr.n_levels());
}


void Chunk_manager::init_chunks()
{
	size_t n_chunks = this->prepr.n_chunks();

	this->time_dirty.set_n_items(n_chunks);
	this->state_dirty.set_n_items(n_chunks);
	this->is_active.set_n_items(n_chunks);

	this->state.resize(n_chunks);
	this->time.resize(n_chunks, {TIM_MAX, TIM_MAX});

	this->res_idx.resize(n_chunks);
	this->train_idx.resize(n_chunks);

	for (auto& chunk : this->prepr.chunks) {
		this->res_idx[chunk.idx] = chunk.res;
		this->train_idx[chunk.idx] = chunk.train;
	}
} 


void Chunk_manager::init_res()
{
	this->res_dirty.set_n_items(this->inst.n_res);
	this->res.resize(this->inst.n_res);
	this->res_data.resize(this->prepr.n_chunks());

	auto res_range = this->inst.res_range();
	auto chunk_range = this->prepr.chunks_range();

	for (auto r : res_range) {
		this->res[r].idx = r;
		this->res[r].size = 0;
	}

	for (auto c : chunk_range) {
		this->res[this->res_idx[c]].size += 1;
	}

	size_t data_idx = 0;
	for (auto& res : this->res) {
		res.chunks = &this->res_data[data_idx];
		data_idx += res.size;
		res.size = 0;
	}

	assert(data_idx == this->prepr.n_chunks());

	for (auto c : chunk_range) {
		auto& res = this->res[this->res_idx[c]];
		res.chunks[res.size++] = c;
	}
};


void Chunk_manager::op_change(const Flag& op_change_flag)
{
	op_change_flag.get_true_list(this->flag_list);
	for (auto c : this->flag_list) {
		for (auto c : this->prepr.ops[c].chunks) {
			this->state_dirty += c;
		}
	}
}


void Chunk_manager::time_change(const Flag& level_time_change)
{
	level_time_change.get_true_list(this->flag_list);
	for (auto& l : this->flag_list) {
		for (auto c : this->level_chunks[l]) {
			this->time_dirty += c;
		}
	}
}


void Chunk_manager::sync_state(const Flag& op_active)
{
	this->state_dirty.get_true_list(this->flag_list);
	for (auto c : this->flag_list) {
		auto& chunk = this->prepr.chunks[c];
		
		this->is_active -= c;
		State new_state = {{IDX_MAX, IDX_MAX}, 0};

		for (auto o : chunk.ops) {
			if (!op_active[o]) {
				continue;
			}

			this->is_active += c;
			auto& op = this->prepr.ops[o];
			
			new_state.level.start = (new_state.level.start == IDX_MAX) ? 
				op.level.start : new_state.level.start;

			new_state.level.end = op.level.end;
			new_state.dur = op.inst->res.find_asc(chunk.res)->dur;
		}

		auto& state = this->state[c];
		if (state != new_state) {
			this->update_level_chunks(c, state.level.start, new_state.level.start);
			this->update_level_chunks(c, state.level.end, new_state.level.end);

			state = new_state;
		}
		else {
			this->state_dirty -= c;
		}
	}

	this->time_dirty.set_true(this->state_dirty);
	this->state_dirty.clear();
}


void Chunk_manager::sync_time(const vector<tim_t>& level_time)
{
	this->time_dirty.get_true_list(this->flag_list);
	for (auto c : this->flag_list) {
		auto& state = this->state[c];
		auto& time = this->time[c];

		Interval<tim_t> new_time;

		if (this->is_active[c]) {
			new_time.start = level_time[state.level.start];
			new_time.end = level_time[state.level.end] + state.dur;
		}
		else {
			new_time = {TIM_MAX, TIM_MAX};
		}

		if (time != new_time) {
			time = new_time;
			this->res_dirty += this->res_idx[c];
		}
	}

	this->time_dirty.clear();
	this->sync_res();
}


void Chunk_manager::sync_res()
{
	auto time_cmp = Time_cmp(this->time);

	this->res_dirty.get_true_list(this->flag_list);
	for (auto r : this->flag_list) {
		auto& res = this->res[r];
		sort(res.chunks, &res.chunks[res.size], time_cmp);
	}

	this->res_dirty.clear();
}

void Chunk_manager::get_all_conflicts(vector<idx_pr>& confs, double stretch)
{
	confs.clear();

	for (auto r : this->inst.res_range()) {
		auto& res = this->res[r];

		for (idx_t i = 0; i < res.size; i++) {
			idx_t c_a = res.chunks[i];
			auto& t_a = this->time[c_a];

			if (t_a.start == TIM_MAX || t_a.end == TIM_MAX) {
				break;
			}

			tim_t d_a = round(stretch*(t_a.end - t_a.start));

			for (idx_t j = i + 1; j < res.size; j++) {
				idx_t c_b = res.chunks[j];

				if (this->train_idx[c_a] == this->train_idx[c_b]) {
					continue;
				}

				auto& t_b = this->time[c_b];

				if (t_b.start == TIM_MAX || t_b.end == TIM_MAX) {
					break;
				}

				tim_t d_b = round(stretch*(t_b.end - t_b.start));
			
				if (t_a.end + d_a + d_b > t_b.start) {
					confs.push_back({c_a, c_b});
				}
				else {
					break;
				}
			}
 		}
	}
}


Preprocess::idx_pr Chunk_manager::get_earliest_conflict()
{
	idx_pr min_conf = {IDX_MAX, IDX_MAX};
	tim_t min_time = TIM_MAX;

	for (auto r : this->inst.res_range()) {
		auto& res = this->res[r];

		for (idx_t i = 0; i < res.size; i++) {
			idx_t c_a = res.chunks[i];
			auto& t_a = this->time[c_a];

			if (t_a.start == TIM_MAX || t_a.end <= min_time) {
				break;
			}

			for (idx_t j = i + 1; j < res.size; j++) {
				idx_t c_b = res.chunks[j];

				if (this->train_idx[c_a] == this->train_idx[c_b]) {
					continue;
				}

				auto& t_b = this->time[c_b];

				if (t_b.start <= min_time || t_b.end == TIM_MAX) {
					break;
				}
			
				if (t_a.end > t_b.start) {
					min_time = t_b.start;
					min_conf = {c_a, c_b};
				}
				else {
					break;
				}
			}
 		}
	}

	return min_conf;

}


bool Chunk_manager::update_level_chunks(idx_t c, idx_t l_old, idx_t l_new)
{
	if (l_old == l_new) {
		return false;
	}

	if (l_old < IDX_MAX) {
		auto& x = this->level_chunks[l_old];
		
		auto it = find(x.begin(), x.end(), c);
		assert(it != x.end());
		
		*it = x.back();
		x.pop_back();
	}

	if (l_new < IDX_MAX) {
		this->level_chunks[l_new].push_back(c);
	}

	return true;
}




