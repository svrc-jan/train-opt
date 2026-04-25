#include "chunk_manager.hpp"

#include <iostream>
#include <algorithm>

#include "utils/stl_print.hpp"

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
	this->link.resize(n_chunks);
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
		this->res[r].cap = 0;
	}

	for (auto c : chunk_range) {
		this->res[this->res_idx[c]].cap += 1;
	}

	size_t data_idx = 0;
	for (auto& res : this->res) {
		res.chunks = &this->res_data[data_idx];
		data_idx += res.cap;
		res.cap = 0;
	}

	assert(data_idx == this->prepr.n_chunks());

	for (auto c : chunk_range) {
		auto& res = this->res[this->res_idx[c]];
		res.chunks[res.cap++] = c;
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
		auto& chunk = this->prepr.chunks[c];
		
		Chunk_state new_state = {{IDX_MAX, IDX_MAX}, 0};

		

		this->is_active -= c;

		this->new_link_fwd.clear();
		this->new_link_bkwd.clear();

		for (auto o : chunk.ops) {
			if (!this->slvr.route_plnr->is_op_active[o.idx]) {
				continue;
			}

			this->is_active += c;

			auto& op = this->prepr.ops[o.idx];
			
			new_state.level.start = (new_state.level.start == IDX_MAX) ? 
				op.level.start : new_state.level.start;

			new_state.level.end = op.level.end;
			new_state.rel_time = o.dur;

			

			for (auto x : op.chunks) {
				if (x == c) { continue; }
				this->new_link_fwd.insert(x);
				this->new_link_bkwd.insert(x);
			}

			for (auto p : op.inst->pred) {
				if (!this->slvr.route_plnr->is_op_active[p]) {
					continue;
				}

				for (auto x : this->prepr.ops[p].chunks) {
					if (x == c) { continue; }
					this->new_link_bkwd.insert(x);
				}
			}

			for (auto s : op.inst->succ) {
				if (!this->slvr.route_plnr->is_op_active[s]) {
					continue;
				}

				for (auto x : this->prepr.ops[s].chunks) {
					if (x == c) { continue; }
					this->new_link_fwd.insert(x);
				}
			}
		}

		auto& state = this->state[c];
		if (state != new_state) {
			this->update_level_chunks(c, state.level.start, new_state.level.start);
			this->update_level_chunks(c, state.level.end, new_state.level.end);

			state = new_state;

			this->time_dirty += c;
			this->need_time_sync = true;

			this->slvr.conf_rslvr->chunk_state_change(c);
		}

		auto& link = this->link[c];
		if (link.fwd != this->new_link_fwd) {
			for (auto x : link.bkwd) {
				this->link[x].fwd.erase(c);
				this->slvr.conf_rslvr->chunk_link_change(x);
			}

			link.fwd = this->new_link_fwd;
			this->slvr.conf_rslvr->chunk_link_change(c);

			for (auto x : link.bkwd) {
				this->link[x].fwd.insert(c);
				this->slvr.conf_rslvr->chunk_link_change(x);
			}
		}

		if (link.bkwd != this->new_link_bkwd) {
			for (auto x : link.bkwd) {
				this->link[x].bkwd.erase(c);
				this->slvr.conf_rslvr->chunk_link_change(x);
			}

			link.bkwd = this->new_link_bkwd;
			this->slvr.conf_rslvr->chunk_link_change(c);

			for (auto x : link.bkwd) {
				this->link[x].bkwd.insert(c);
				this->slvr.conf_rslvr->chunk_link_change(x);
			}
		}
	}

	this->state_dirty.clear();
	this->need_state_sync = false;
}


void Chunk_manager::print_links()
{
	for (auto c : this->prepr.chunks_range()) {
		if (this->print_chunk_link(c)) {
			cout << endl;
		}
	}
}


bool Chunk_manager::print_chunk_link(idx_t c)
{
	auto& link = this->link[c];
	if (link.fwd.empty() && link.bkwd.empty()) { return false; }

	cout << c; 
	if (!link.fwd.empty()) {
		cout << " fwd: " << link.fwd;
	}
	if (!link.bkwd.empty()) {
		cout << " bkwd: " << link.bkwd;
	}

	return true;
}


void Chunk_manager::sync_time()
{
	this->sync_state();
	this->slvr.sync_graph();

	if (!this->need_time_sync) {
		return;
	}

	this->time_dirty.get_true_list(this->slvr.need_list);
	for (auto c : this->slvr.need_list) {
		auto& state = this->state[c];
		auto& time = this->time[c];

		Interval<tim_t> new_time;

		if (this->is_active[c]) {
			new_time.start = this->slvr.time(state.level.start);
			new_time.end = this->slvr.time(state.level.end) + state.rel_time;
		}
		else {
			new_time = {TIM_MAX, TIM_MAX};
		}

		if (time != new_time) {
			time = new_time;

			this->res_dirty += this->res_idx[c];
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

	auto time_cmp = Time_cmp(this->time);

	this->res_dirty.get_true_list(this->slvr.need_list);
	for (auto r : this->slvr.need_list) {
		auto& res = this->res[r];

		size_t i = 0;
		res.size = res.cap;

		while (i < res.size) {
			idx_t c = res.chunks[i];
			if (this->is_active[c]) {
				assert((this->time[c].start < TIM_MAX) && (this->time[c].end < TIM_MAX));
				i++;
			}
			else {
				assert(this->time[c] == Interval<tim_t>(TIM_MAX, TIM_MAX));
				swap(res.chunks[i], res.chunks[res.size - 1]);
				res.size--;
			}
		}

		sort(res.chunks, &res.chunks[res.size], time_cmp);
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




