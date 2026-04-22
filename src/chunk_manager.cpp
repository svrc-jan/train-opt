#include "chunk_manager.hpp"


using namespace std;


Chunk_manager::Chunk_manager(Solver& solver)
	: inst(solver.inst), prepr(solver.prepr), slvr(solver)
{
	
}


Chunk_manager::~Chunk_manager()
{

}


void Chunk_manager::init_data()
{
	this->init_levels();
	this->init_chunks();
	this->init_res();
}


void Chunk_manager::init_chunks()
{
	size_t n_chunks = this->prepr.n_chunks();

	this->time_dirty.set_n_items(n_chunks);
	this->state_dirty.set_n_items(n_chunks);

	this->chunks.resize(n_chunks);
	for (auto& chunk : this->prepr.chunks) {
		this->chunks[chunk.idx].prepr = &chunk;
	}
}


void Chunk_manager::init_res()
{
	this->res_dirty.set_n_items(this->inst.n_res);
	this->res.resize(this->inst.n_res);
	this->res_data.resize(this->prepr.n_chunks());

	auto res_range = this->inst.res_range();

	for (auto r : res_range) {
		this->res[r].set_size(0);
	}

	for (auto& chunk : this->prepr.chunks) {
		this->res[chunk.res].increment_size(1);
	}

	size_t res_chunk_idx = 0;
	for (auto r : res_range) {
		res[r].assign_offset(this->res_data, res_chunk_idx, true);
	}
	assert(res_chunk_idx == this->prepr.n_chunks());

	for (auto& chunk : this->chunks) {
		res[chunk.prepr->res].push_back(&chunk);
	}
};


void Chunk_manager::sync_state()
{
	this->slvr.route_plnr->sync_route_ops();
	if (!this->need_state_sync) {
		return;
	}

	this->state_dirty.get_true_list(this->slvr.need_list);

	for (auto c : this->slvr.need_list) {
		Chunk& chunk = this->chunks[c];
		Chunk_state new_state;

		for (auto chunk_op : chunk.prepr->ops) {
			if (this->slvr.route_plnr->is_op_active[chunk_op.idx]) {
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
			this->time_dirty += chunk.prepr->idx;
		}
	}

	this->state_dirty.clear();
	this->need_state_sync = false;
}


void Chunk_manager::sync_time()
{
	this->sync_state();
	this->slvr.sync_graph_time();

	if (!this->need_time_sync) {
		return;
	}


	this->time_dirty.get_true_list(this->slvr.need_list);
	for (auto c : this->slvr.need_list) {
		Chunk& chunk = this->chunks[c];

		Interval<tim_t> new_time;

		if (chunk.is_active()) {
			new_time.start = this->slvr.time(chunk.state.level.start);
			new_time.end = this->slvr.time(chunk.state.level.end) +
				chunk.state.rel_time;
		}
		else {
			new_time = {TIM_MAX, TIM_MAX};
		}

		if (chunk.time != new_time) {
			chunk.time = new_time;

			this->res_dirty += chunk.prepr->res;
			this->need_res_sync = true;
		}
	}

	this->time_dirty.clear();
	this->need_time_sync = false;
}


void Chunk_manager::sync_res()
{
	this->sync_time();

	if (!this->need_res_sync) {
		return;
	}

	this->res_dirty.get_true_list(this->slvr.need_list);
	for (auto r : this->slvr.need_list) {
		auto& rc = this->res[r];

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

	this->res_dirty.clear();
	this->need_res_sync = false;
}



bool Chunk_manager::update_level_chunks(idx_t c, idx_t l_old, idx_t l_new)
{
	if (l_old == l_new) {
		return false;
	}

	if (l_old < IDX_MAX) {
		auto ret = this->level_chunks[l_old].erase(c);
		assert(ret);
	}

	else {
		auto ret = this->level_chunks[l_new].insert(c);
		assert(ret.second);
	}

	return true;
}


