#include "graph.hpp"

#include <print>
#include <iostream>
#include "utils/stl_print.hpp"

using namespace std;

Graph::Graph(const Preprocess& prepr)
	: inst(prepr.inst), prepr(prepr)
{
	this->set_num_vertices(prepr.n_levels());
	this->clear_constrains();
}


void Graph::set_num_vertices(const int n_ver)
{
	this->n_ver = n_ver;

	this->time.resize(n_ver, 0);
	this->forward.resize(n_ver, Vertex_forward());
	this->backward.resize(n_ver, Vertex_backward());
	this->time_bounds.resize(n_ver, Vertex_time_bounds());

	this->dirty.resize_for_items(this->n_ver);
	this->visited.resize_for_items(this->n_ver);

	this->dirty.clear();
}


void Graph::clear_constrains()
{
	for (auto& vtx_forward : this->forward) {
		vtx_forward.constrains.clear();
	}

	for (auto& vtx_backward : this->backward) {
		vtx_backward.constrains.clear();
	}
}


void Graph::set_all_paths(const vector<vector<int>>& paths)
{
	for (auto& path : paths) {
		this->set_path(path);
	}
}


void Graph::set_path(const vector<int>& path)
{
	for (int i = 1; i < (int)path.size(); i++) {
		assert(this->prepr.op_level[path[i-1]].second == this->prepr.op_level[path[i]].first);
	}

	// assume path is valid, no checking is done
	for (int o : path) {
		this->set_path_op(o);
	}

	int v_path_start = this->prepr.op_level[path[0]].first;
	int v_path_end = this->prepr.op_level[path.back()].second;

	assert(this->prepr.levels[v_path_start].ops_in.size == 0);
	assert(this->prepr.levels[v_path_end].ops_out.size == 0);

	this->backward[v_path_start].path = {.vertex = -1, .time = 0};
	this->forward[v_path_end].path = -1;

	this->mark_dirty_rec(v_path_start);
}


void Graph::set_path_op(const int op)
{
	auto& inst_op = this->inst.ops[op];

	int v_start = this->prepr.op_level[op].first;
	int v_end = this->prepr.op_level[op].second;

	auto& vtx_forward = this->forward[v_start];
	auto& vtx_backward = this->backward[v_end];
	auto& vtx_time_bounds = this->time_bounds[v_start];

	vtx_forward.path = v_end;
	vtx_backward.path = {.vertex = v_start, .time = (TIME_T)inst_op.dur};
	vtx_time_bounds = {.lower = inst_op.start_lb, .upper = inst_op.start_ub};
}


bool Graph::add_edge(const Edge& edge)
{
	this->visited.clear();
	if (!this->find_visited_rec(edge.vertex_to, edge.vertex_from)) {
		return false;
	}

	this->dirty.update(this->visited);

	this->forward[edge.vertex_from].constrains.push_back(edge.vertex_to);
	this->backward[edge.vertex_to].constrains.push_back({
		.vertex = edge.vertex_from,
		.time = edge.time
	});

	return true;
}

bool Graph::remove_edge(const Edge& edge)
{
	auto& forward_cons = this->forward[edge.vertex_from].constrains;
	auto& backward_cons = this->backward[edge.vertex_to].constrains;

	int forward_idx = -1;
	for (int i = (int)forward.size() - 1; i >= 0; i--) {
		if (forward_cons[i] == edge.vertex_to) {
			forward_idx = i;
			break;
		}
	}

	int backward_idx = -1;
	for (int i = (int)backward.size() - 1; i >= 0; i--) {
		if (backward_cons[i].vertex == edge.vertex_from 
			&& backward_cons[i].time == edge.time) {
			
			backward_idx = i;
			break;
		}
	}

	if (forward_idx < 0 || backward_idx < 0) {
		return false;
	}

	this->mark_dirty_rec(edge.vertex_from);

	return true;
};

bool Graph::find_visited(const int v_begin, const int v_cycle)
{
	this->visited.clear();

	this->visited.set_true(v_begin);
	this->dq.push_back(v_begin);

	while (!this->dq.empty()) {
		int v_curr = this->dq.front();
		this->dq.pop_front();

		const auto& vtx_forward = this->forward[v_curr];

		int v_path = vtx_forward.path;
		if (v_path >= 0) {
			if (v_path == v_cycle) {
				return false;
			}

			if (!this->visited.get(v_path)) {
				this->visited.set_true(v_path);
				this->dq.push_back(v_path);
			}
		}

		for (int v_cons : vtx_forward.constrains) {
			if (v_cons == v_cycle) {
				return false;
			}

			if (!this->visited.get(v_cons)) {
				this->visited.set_true(v_cons);
				this->dq.push_front(v_cons); // prioritize resource branching to find cycles
			}
		}
	}

	return true;
}


void Graph::mark_dirty(const int v_begin)
{
	this->dirty.set_true(v_begin);
	this->dq.push_back(v_begin);

	while (!this->dq.empty()) {
		int v_curr = this->dq.front();
		this->dq.pop_front();

		const auto& vtx_forward = this->forward[v_curr];

		int v_path = vtx_forward.path;
		if (v_path >= 0) {
			if (!this->dirty.get(v_path)) {
				this->dirty.set_true(v_path);
				this->dq.push_front(v_path);
			}
		}

		for (int v_cons : vtx_forward.constrains) {
			if (!this->dirty.get(v_cons)) {
				this->dirty.set_true(v_cons);
				this->dq.push_back(v_cons);
			}
		}
	}
}


void Graph::update_time(const int v_begin)
{
	// backward pass to find all dirty predecessors
	this->visited.clear();
	this->need_update.clear();

	this->visited.set_true(v_begin);
	this->need_update.push_back(v_begin);
	this->dq.push_back(v_begin);

	while (!this->dq.empty()) {
		int v_curr = this->dq.front();
		this->dq.pop_front();

		const auto& vtx_backward = this->backward[v_curr];

		int v_path = vtx_backward.path.vertex;
		if (v_path >= 0) {
			if (!this->visited.get(v_path) && this->dirty.get(v_path)) {
				this->visited.set_true(v_path);
				this->need_update.push_back(v_path);
				this->dq.push_front(v_path);
			}
		}

		for (const auto& cons : vtx_backward.constrains) {
			int v_cons = cons.vertex;
			if (!this->visited.get(v_cons) && this->dirty.get(v_cons)) {
				this->visited.set_true(v_cons);
				this->need_update.push_back(v_cons);
				this->dq.push_back(v_cons);
			}
		}
	}

	// cout << this->need_update << endl;

#ifndef NO_VLA
	uint8_t n_dep[this->n_ver];
#else
	vector<uint8_t> n_dep(this->n_ver);
#endif

	// calculate number of dependencies
	for (int v : this->need_update) {
		n_dep[v] = 0;
	}

	for (int v : this->need_update) {
		const auto& vtx_forward = this->forward[v];

		int v_path = vtx_forward.path;
		if (v_path >= 0) {
			if (this->visited.get(v_path)) {
				n_dep[v_path] += 1;
			}
		}

		for (int v_cons : vtx_forward.constrains) {
			if (this->visited.get(v_cons)) {
				n_dep[v_cons] += 1;
			}
		}
	}

	for (int v : this->need_update) {
		if (n_dep[v] == 0) {
			this->dq.push_back(v);
		};
	}

	// forward pass to update times from previous
	while (!this->dq.empty()) {
		int v_curr = this->dq.front();
		this->dq.pop_front();

		const auto& vtx_forward = this->forward[v_curr];
		const auto& vtx_backward = this->backward[v_curr];
		const auto& vtx_time_bounds = this->time_bounds[v_curr];

		// backward time updates
		this->time[v_curr] = vtx_time_bounds.lower;

		auto& path = vtx_backward.path;
		if (path.vertex >= 0) {
			assert(!this->dirty.get(path.vertex));
			this->time[v_curr] = max(this->time[v_curr], 
				this->time[path.vertex] + path.time);
		}

		for (const auto& cons : vtx_backward.constrains) {
			assert(!this->dirty.get(cons.vertex));
			this->time[v_curr] = max(this->time[v_curr], 
				this->time[cons.vertex] + cons.time);
		}

		this->dirty.set_false(v_curr);

		// forward enque necessary
		int v_path = vtx_forward.path;
		if (v_path >= 0) {
			if (this->visited.get(v_path)) {
				assert(this->dirty.get(v_path));
				n_dep[v_path] -= 1;
				if (n_dep[v_path] == 0) {
					this->dq.push_front(v_path); // prioritize path for locallity
				} 
			}
		}

		for (int v_cons: vtx_forward.constrains) {
			if (this->visited.get(v_cons)) {
				assert(this->dirty.get(v_cons));
				n_dep[v_cons] -= 1;
				if (n_dep[v_cons] == 0) {
					this->dq.push_back(v_cons);
				} 
			}
		}
	}
	
	// check if the target node is updated
	assert(!this->dirty.get(v_begin));
}


bool Graph::find_visited_rec(const int v, const int v_cycle)
{
	if (v == v_cycle) {
		return false;
	}

	if (this->visited.get(v)) {
		return true;
	}

	this->visited.set_true(v);
	const auto& vtx_forward = this->forward[v];

	for (int v_cons : vtx_forward.constrains) {
		if (!this->find_visited_rec(v_cons, v_cycle)) {
			return false;
		}
	}

	int v_path = vtx_forward.path;
	if (v_path >= 0) {
		if (!this->find_visited_rec(v_path, v_cycle)) {
			return false;
		}
	}

	return true;
}


void Graph::mark_dirty_rec(const int v)
{
	if (this->dirty.get(v)) {
		return;
	}

	this->dirty.set_true(v);

	const auto& vtx_forward = this->forward[v];

	int v_path = vtx_forward.path;
	if (v_path >= 0) {
		this->mark_dirty_rec(v_path);
	}

	for (int v_cons : vtx_forward.constrains) {
		this->mark_dirty_rec(v_cons);
	}

	
}


void Graph::update_time_rec(const int v)
{
	if (!this->dirty.get(v)) {
		return;
	}

	const auto& vtx_backward = this->backward[v];

	TIME_T new_time;
	new_time = this->time_bounds[v].lower;

	auto &path = vtx_backward.path;
	if (path.vertex >= 0) {
		this->update_time_rec(path.vertex);
		new_time = max(new_time, this->time[path.vertex] + path.time);
	}

	for (auto& cons : vtx_backward.constrains) {
		this->update_time_rec(cons.vertex);
		new_time = max(new_time, this->time[cons.vertex] + cons.time);
	}

	this->time[v] = new_time;
	this->dirty.set_false(v);
}

void Graph::update_time_visited_rec(const int v)
{
	if (this->visited.get(v)) {
		return;
	}

	const auto& vtx_backward = this->backward[v];

	TIME_T new_time;
	new_time = this->time_bounds[v].lower;

	auto &path = vtx_backward.path;
	if (path.vertex >= 0) {
		this->update_time_rec(path.vertex);
		new_time = max(new_time, this->time[path.vertex] + path.time);
	}

	for (auto& cons : vtx_backward.constrains) {
		this->update_time_rec(cons.vertex);
		new_time = max(new_time, this->time[cons.vertex] + cons.time);
	}

	this->time[v] = new_time;
	this->visited.set_true(v);
}