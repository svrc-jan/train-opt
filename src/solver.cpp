#include "solver.hpp"

#include <cmath>
#include "utils/stl_print.hpp"

using namespace std;


Solver::Solver(const Preprocess& prepr, GRBEnv& grb_env)
	: inst(prepr.inst), prepr(prepr), grb_env(grb_env)
{

	this->route_plnr = unique_ptr<Route_planner>(new Route_planner(*this));
	this->conf_rslvr = unique_ptr<Conflict_resolver>(new Conflict_resolver(*this));

	this->event_graph.set_n_vtx(prepr.n_levels());
	this->init_data();
}


Solver::~Solver()
{

}


void Solver::solve()
{
	this->route_plnr->assign_all_random_sections();
	this->sync_res_chunks();
}


void Solver::init_data()
{
	this->level_time_dirty.set_n_items(this->prepr.n_levels());
	this->init_chunks();

	this->route_plnr->init_data();
	this->conf_rslvr->init_data();
}


void Solver::init_chunks()
{
	this->chunk_time_dirty.set_n_items(this->prepr.n_chunks());
	this->chunk_state_dirty.set_n_items(this->prepr.n_chunks());

	this->chunks.resize(this->prepr.n_chunks());
	for (auto& chunk : this->prepr.chunks) {
		this->chunks[chunk.idx].prepr = &chunk;
	}

	this->init_res_chunks();
}


void Solver::init_res_chunks()
{
	this->res_chunks_dirty.set_n_items(this->inst.n_res);
	this->res_chunks.resize(this->inst.n_res);
	this->res_chunks_data.resize(this->prepr.n_chunks());

	auto res_range = this->inst.res_range();

	size_t chunk_cnt[this->inst.n_res];

	for (auto r : res_range) {
		this->res_chunks[r].set_size(0);
		chunk_cnt[r] = 0;
	}

	for (auto& chunk : this->prepr.chunks) {
		chunk_cnt[chunk.res] += 1;
	}

	size_t res_chunk_idx = 0;
	for (auto r : res_range) {
		res_chunks[r].assign_offset(this->res_chunks_data, res_chunk_idx, true);
	}
	assert(res_chunk_idx == this->prepr.n_chunks());

	for (auto& chunk : this->chunks) {
		res_chunks[chunk.prepr->res].push_back(&chunk);
	}
};


void Solver::sync_level_times()
{
	if (!this->need_level_time_sync) {
		return;
	}

	this->route_plnr->sync_op_graph();

	this->level_time_dirty.clear();
	this->event_graph.sync_time(this->level_time_dirty);
	
	level_time_dirty.get_true_list(this->need_list);
	for (auto l : this->need_list) {
		for (auto c : this->level_chunks[l]) {
			chunk_time_dirty += c;
			need_chunk_time_sync = true;
		}
	}

	this->need_level_time_sync = false;
}


void Solver::sync_chunk_state()
{
	if (!this->need_chunk_state_sync) {
		return;
	}

	this->route_plnr->sync_route_ops();
	this->chunk_state_dirty.get_true_list(this->need_list);

	for (auto c : this->need_list) {
		Chunk& chunk = this->chunks[c];
		Chunk_state new_state;

		for (auto chunk_op : chunk.prepr->ops) {
			if (this->route_plnr->is_op_active[chunk_op.idx]) {
				auto& op = this->prepr.ops[chunk_op.idx];
				
				new_state.level.start = (new_state.level.start == IDX_MAX) ? 
					op.level.start : new_state.level.start;

				new_state.level.end = op.level.end;
				new_state.rel_time = chunk_op.rel_time;
			}
		}

		if (chunk.state != new_state) {
			this->update_level_chunks(chunk.prepr->idx, 
				chunk.state.level.start, new_state.level.start);

			this->update_level_chunks(chunk.prepr->idx, 
				chunk.state.level.end, new_state.level.end);

			chunk.state = new_state;
			this->chunk_time_dirty += chunk.prepr->idx;
		}
	}

	this->chunk_state_dirty.clear();
	this->need_chunk_state_sync = false;
}


void Solver::update_level_chunks(idx_t c, idx_t l_old, idx_t l_new)
{
	if (l_old == l_new) {
		return;
	}

	if (l_old < IDX_MAX) {
		auto ret = this->level_chunks[l_old].erase(c);
		assert(ret);
	}

	else {
		auto ret = this->level_chunks[l_new].insert(c);
		assert(ret.second);
	}
}


void Solver::sync_chunk_times()
{
	if (!this->need_chunk_time_sync) {
		return;
	}

	this->sync_chunk_state();
	this->sync_level_times();

	this->chunk_time_dirty.get_true_list(this->need_list);
	for (auto c : this->need_list) {
		Chunk& chunk = this->chunks[c];

		Interval<tim_t> new_time;

		if (chunk.is_active()) {
			new_time.start = this->event_graph.time[chunk.state.level.start];
			new_time.end = this->event_graph.time[chunk.state.level.end] +
				chunk.state.rel_time;
		}
		else {
			new_time = {TIM_MAX, TIM_MAX};
		}

		if (chunk.time != new_time) {
			chunk.time = new_time;

			this->res_chunks_dirty += chunk.prepr->res;
			this->need_res_chunks_sync = true;
		}
	}

	this->chunk_time_dirty.clear();
	this->need_chunk_time_sync = false;
}


void Solver::sync_res_chunks()
{
	if (!this->need_res_chunks_sync) {
		return;
	}

	this->sync_chunk_times();

	this->res_chunks_dirty.get_true_list(this->need_list);
	for (auto r : this->need_list) {
		auto& rc = this->res_chunks[r];

		size_t i = 0;
		size_t j = rc.size() - 1;

		while (i < j) {
			if (!rc[i]->has_active_time()) {
				swap(rc[i], rc[j]);
				j--;
			}
			else {
				assert(rc[i]->is_active());
				i++;
			}
		}

		sort(&rc[0], &rc[j], Chunk::ptr_cmp);
	}


	this->res_chunks_dirty.clear();
	this->need_res_chunks_sync = false;
}