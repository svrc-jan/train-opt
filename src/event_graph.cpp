#include "event_graph.hpp"

#include <cassert>
#include <iostream>

using namespace std;


Event_graph::Event_graph(const size_t n_vtx)
{
	if (n_vtx > 0) {
		this->set_n_vtx(n_vtx);
	}
}


void Event_graph::set_n_vtx(const size_t n_vtx)
{
	assert(n_vtx < VTX_MAX);
	this->n_vtx = n_vtx;

	this->edges_in.resize(n_vtx, {});
	this->edges_out.resize(n_vtx, {});

	this->visited.set_n_items(n_vtx);
	this->rec_stack.set_n_items(n_vtx);

	this->vtx_dirty.set_n_items(n_vtx);

	this->cycle_pred.resize(n_vtx);
	this->time.resize(n_vtx);
	this->time_lb.resize(n_vtx, 0);
	this->time_pred.resize(n_vtx);
	
	this->update_stack.reserve(n_vtx);
}


bool Event_graph::set_time_lb(vtx_t v, tim_t lb)
{
	bool diff = this->time_lb[v] != lb;
	this->time_lb[v] = lb;
	
	if (diff) {
		this->vtx_dirty += v;
		this->need_sync = true;
	}

	return diff;
}


void Event_graph::add_edge(const Edge& x)
{
	assert(x.is_valid());
	assert(x.v.start != x.v.end);

	this->add_in_entry(x);
	this->add_out_entry(x);

	this->vtx_dirty += x.v.end;
	this->need_sync = true;
}


void Event_graph::remove_edge(const Edge& x)
{
	assert(x.is_valid());
	
	this->remove_in_entry(x);
	this->remove_out_entry(x);

	this->vtx_dirty += x.v.end;
	this->need_sync = true;
}

void Event_graph::add_edges(const std::vector<Edge>& vec)
{
	for (auto& x : vec) {
		this->add_edge(x);
	}
}


void Event_graph::remove_edges(const std::vector<Edge>& vec)
{
	for (auto& x : vec) {
		this->remove_edge(x);
	}
}


void Event_graph::update_edge(const Edge& x_old, const Edge& x_new)
{
	if (x_old == x_new || (!x_old.is_valid() && !x_new.is_valid())) {
		return;
	}
	
	if (!x_old.is_valid()) {
		this->add_edge(x_new);
		return;
	}

	if (!x_new.is_valid()) {
		this->remove_edge(x_old);
		return;
	}

	if (x_old.v.end == x_new.v.end) {
		this->update_in_entry(x_old, x_new);
	}
	else {
		this->remove_in_entry(x_old);
		this->add_in_entry(x_new);
	}

	
	if (x_old.v.start == x_new.v.start) {
		this->update_out_entry(x_old, x_new);
	}
	else {
		this->remove_in_entry(x_old);
		this->remove_out_entry(x_new);
	}
}


void Event_graph::add_in_entry(const Edge& x)
{
	this->edges_in[x.v.end].push_back(x.to_in());
}


void Event_graph::add_out_entry(const Edge& x)
{
	this->edges_out[x.v.start].push_back(x.to_out());
}


void Event_graph::remove_in_entry(const Edge& x)
{
	auto& edg = this->edges_in[x.v.end];

	auto it = find(edg.begin(), edg.end(), x.to_in());
	assert(it != edg.end());

	*it = edg.back();
	edg.pop_back();
}


void Event_graph::remove_out_entry(const Edge& x)
{
	auto& edg = this->edges_out[x.v.start];

	auto it = find(edg.begin(), edg.end(), x.to_out());
	assert(it != edg.end());

	*it = edg.back();
	edg.pop_back();
}


void Event_graph::update_in_entry(const Edge& x_old, const Edge& x_new)
{
	assert(x_old.v.end == x_new.v.end);

	auto& edg = this->edges_in[x_old.v.end];

	auto it = find(edg.begin(), edg.end(), x_old.to_in());
	assert(it != edg.end());

	*it = x_new.to_in();
}


void Event_graph::update_out_entry(const Edge& x_old, const Edge& x_new)
{
	assert(x_old.v.start == x_new.v.start);

	auto& edg = this->edges_in[x_old.v.start];

	auto it = find(edg.begin(), edg.end(), x_old.to_out());
	assert(it != edg.end());

	*it = x_new.to_out();
}


Event_graph::Edge_entry* Event_graph::find_entry(
	vector<Edge_entry>& list, const Edge_entry& entry)
{

	int size = list.size();
	for (int i = size - 1; i >= 0; i--) {
		if (list[i] == entry) {
			return &list[i];
		}
	}
	
	return nullptr;
}


void Event_graph::clear_edges()
{
	for (auto& x : this->edges_in) {
		x.clear();
	}

	for (auto& x : this->edges_out) {
		x.clear();
	}
}


Event_graph::Sync_state Event_graph::sync(Flag& time_change)
{
	if (!this->need_sync) {
		return NO_CHANGES;
	}

	this->vtx_dirty.get_true_list(this->need_list);
	this->cycle_found_vtx.clear();

	if (this->need_list.empty()) {
		return NO_CHANGES;
	}

	this->visited.clear();
	this->rec_stack.clear();
	this->update_stack.clear();

	for (vtx_t v : this->need_list) {
		this->sync_dfs(v);
	}

	// for (vtx_t v = 0; v < this->n_vtx; v++) {
	// 	this->sync_dfs(v);
	// }

	if (this->cycle_found_vtx.size() > 0) {
		return CYCLE_FOUND;
	}


	bool time_changed = false;
	for (vtx_t v : this->update_stack | views::reverse) {
		if (this->sync_time_clear(v, time_change)) {
			time_changed = true;
		};
	}


	return (time_changed ? TIME_UPDATE : NO_CHANGES);
}


bool Event_graph::sync_dfs(vtx_t v)
{
	if (this->rec_stack[v]) {
		this->cycle_found_vtx.push_back(v);
		return false;
	}

	if (this->visited[v]) {
		return true;
	}

	this->visited += v;
	this->rec_stack += v;

	for (const auto& x : this->edges_out[v]) {
		bool ret = sync_dfs(x.v);
		if (!ret) {
			this->rec_stack -= v;
			return false;
		}
	}

	this->update_stack.push_back(v);
	this->rec_stack -= v;

	return true;
}


bool Event_graph::sync_time_clear(vtx_t v, Flag& time_change)
{
	assert(this->visited[v]);

	tim_t old_time = this->time[v];

	this->time[v] = this->time_lb[v];
	this->time_pred[v] = Vertex_edge();

	for (auto& x : this->edges_in[v]) {
		assert(!this->visited[x.v]);
		tim_t new_time = this->time[x.v] + (tim_t)x.d;
		if (this->time[v] < new_time) {
			this->time[v] = new_time;
			this->time_pred[v] = x;
		}
		if (this->time[v] == new_time && x.e == EDG_MAX) {
			this->time_pred[v] = x;
		}
	}

	this->visited -= v;

	if (old_time != this->time[v]) {
		time_change += v;
		return true;
	}

	return false;
}


void Event_graph::get_cycle_path(vector<Vertex_edge>& ret, vtx_t target)
{
	ret.clear();

	while (!this->queue_.empty()) { this->queue_.pop(); }
	this->visited.clear();

	this->queue_.push(target);
	
	bool found = false;

	while (!found && !this->queue_.empty()) {
		vtx_t v = this->queue_.front(); this->queue_.pop();
		
		for (auto& x : this->edges_out[v]) {
			if (!visited[x.v]) {
				this->cycle_pred[x.v] = {v, x.e};
				visited += x.v;
				this->queue_.push(x.v);
			}

			if (x.v == target) {
				found = true;
				break;
			}
		}
	}
	assert(found);


	vtx_t v = target;
	do {
		auto pred = this->cycle_pred[v];
		ret.push_back(pred);
		v = pred.v;
		assert(v < this->n_vtx);
	} while(v != target);
}


void Event_graph::get_critical_path(vector<Vertex_edge>& ret, vtx_t start)
{
	ret.clear();

	vtx_t v = start;
	while (true) {
		auto pred = this->time_pred[v];
		if (pred.v == VTX_MAX) {
			break;
		}
		ret.push_back(pred);
		v = pred.v;
	};
}