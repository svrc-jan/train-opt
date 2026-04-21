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

	this->cycle_dirty.set_n_items(n_vtx);
	this->time_dirty.set_n_items(n_vtx);

	this->cycle_pred.resize(n_vtx);
	this->time_lb.resize(n_vtx, 0);
	this->time_pred.resize(n_vtx);
	
	this->update_stack.reserve(n_vtx);
}


bool Event_graph::set_time_lb(vtx_t v, tim_t lb)
{
	bool diff = this->time_lb[v] != lb;
	this->time_lb[v] = lb;
	
	if (diff) {
		this->time_dirty += v;
		this->need_time_sync = true;
	}

	return diff;
}


void Event_graph::add_edge(const Edge& x)
{
	assert(x.is_valid());

	this->edges_in[x.v.end].push_back(x.to_in());
	this->edges_out[x.v.start].push_back(x.to_out());

	this->cycle_dirty += x.v.end;
	this->time_dirty += x.v.end;

	this->need_cycle_sync = true;
	this->need_time_sync = true;
}


bool Event_graph::remove_edge(const Edge& x)
{
	assert(x.is_valid());

	auto& edg_in = this->edges_in[x.v.end];
	auto in_entry = this->find_entry(edg_in, x.to_in());
	
	auto& edg_out = this->edges_out[x.v.start];
	auto out_entry = this->find_entry(edg_out, x.to_out());

	if ((in_entry == nullptr) || (out_entry == nullptr)) {
		assert((in_entry == nullptr) && (out_entry == nullptr));
		return false;
	}

	*in_entry = edg_in.back();
	edg_in.pop_back();

	*out_entry = edg_out.back();
	edg_out.pop_back();

	this->time_dirty += x.v.end;
	this->need_time_sync = true;

	return true;
}

bool Event_graph::update_edge(const Edge& e_old, const Edge& e_new)
{
	if (e_old == e_new) {
		return true;
	}

	bool ret = true;
	if (e_old.v == e_new.v) {
		if (!e_old.is_valid()) {
			return true;
		}
		ret = this->update_edge_same_vtx(e_old, e_new);
		return ret;
	}
	
	if (e_old.is_valid()) {
		ret = this->remove_edge(e_old);
	}

	if (e_new.is_valid()) {
		this->add_edge(e_new);
	}

	return ret;
}


bool Event_graph::update_edge_same_vtx(const Edge& e_old, const Edge& e_new)
{
	assert(e_old.v == e_new.v && e_old.is_valid() && e_new.is_valid());

	auto& edg_in = this->edges_in[e_old.v.end];
	auto in_entry = this->find_entry(edg_in, e_old.to_in());
	
	auto& edg_out = this->edges_out[e_old.v.start];
	auto out_entry = this->find_entry(edg_out, e_old.to_out());

	if ((in_entry == nullptr) || (out_entry == nullptr)) {
		assert((in_entry == nullptr) && (out_entry == nullptr));
		return false;
	}

	*in_entry = e_new.to_in();
	*out_entry = e_new.to_out();

	this->time_dirty += e_new.v.start;
	this->need_time_sync = true;

	return true;
}


Event_graph::Edge_entry* Event_graph::find_entry(
	vector<Edge_entry>& list, const Edge_entry& entry)
{

	int size = list.size();
	for (int i = size - 1; i >= 0; i++) {
		if (list[i] == entry) {
			return &list[i];
		}
	}
	
	return nullptr;
}



void Event_graph::add_edges(const vector<Edge>& edges)
{
	for (auto& x : edges) {
		this->add_edge(x);
	}
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


void Event_graph::remove_last_edges(const std::vector<Edge>& edges)
{
	if (edges.size() == 0) {
		return;
	}

	vtx_t in_count[this->n_vtx];
	vtx_t out_count[this->n_vtx];

	auto vtx_range = Range<vtx_t>(this->n_vtx);

	for (auto v : vtx_range) {
		in_count[v] = 0;
		out_count[v] = 0;
	}

	for (auto& x : edges) {
		in_count[x.v.end] += 1;
		out_count[x.v.start] += 1;	
	}

	for (auto v : vtx_range) {
		vtx_t cnt = in_count[v];
		if (cnt > 0) {
			auto& x = this->edges_in[v];
			x.resize(x.size() - cnt);
			this->time_dirty += v;
		}

		cnt = out_count[v];
		if (cnt > 0) {
			auto& x = this->edges_out[v];
			x.resize(x.size() - cnt);
			this->time_dirty += v;
		}
	}

	this->need_time_sync = true;
}

void Event_graph::remove_last_edges_small(const std::vector<Edge>& edges)
{
	for (auto& x : edges) {
		this->edges_in[x.v.end].pop_back();
		this->edges_out[x.v.start].pop_back();

		this->time_dirty += x.v.start;
		this->time_dirty += x.v.end;
	}
}

bool Event_graph::sync_cycle()
{
	if (!this->need_cycle_sync) {
		return true;
	}

	this->cycle_dirty.get_true_list(this->need_list);

	this->cycle_found_vtx.clear();
	if (this->need_list.empty()) {
		return true;
	}

	this->visited.clear();
	this->rec_stack.clear();
	for (vtx_t v : this->need_list) {
		this->update_cycle_rec(v);
	}

	if (this->cycle_found_vtx.size() > 0) {
		this->update_cycle_paths();
		return false;
	}

	this->cycle_dirty.clear();
	this->need_cycle_sync = false;

	return true;
}


bool Event_graph::update_cycle_rec(vtx_t v)
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
		if(update_cycle_rec(x.v)) {
			this->rec_stack -= v;
			return false;
		}
	}

	this->rec_stack -= v;
	return true;
}


void Event_graph::update_cycle_paths()
{
	while (!this->queue_.empty()) { this->queue_.pop(); }
	this->visited.clear();
	
	for (vtx_t target : this->cycle_found_vtx) {
		this->queue_.push(target);
		
		bool found = false;

		while (!found && !this->queue_.empty()) {
			vtx_t v = this->queue_.front(); this->queue_.pop();
			
			for (auto& x : this->edges_out[v]) {
				if (!visited[x.v]) {
					this->cycle_pred[x.v] = {v, x.e};
					visited += x.v;
				}

				if (v == target) {
					found = true;
					break;
				}
			}
		}
		assert(found);
	}
}


void Event_graph::sync_time(Flag& time_change)
{
	if (!this->need_time_sync) {
		return;
	}
	
	this->time_dirty.get_true_list(this->need_list);

	if (this->need_list.empty()) {
		return;
	}

	this->visited.clear();
	this->rec_stack.clear();
	this->update_stack.clear();

	for (vtx_t v : this->need_list) {
		this->update_time_stack(v);
	}

	for (vtx_t v : this->update_stack | views::reverse) {
		this->update_time_clear(v, time_change);
	}

	this->time_dirty.clear();
	this->need_time_sync = false;
}


void Event_graph::update_time_stack(vtx_t v)
{
	assert (!this->rec_stack[v]);

	if (this->visited[v]) {
		return;
	}

	this->visited += v;
	this->rec_stack += v;

	for (const auto& x : this->edges_out[v]) {
		update_time_stack(x.v);
	}

	this->update_stack.push_back(v);
	this->rec_stack -= v;
}


void Event_graph::update_time_clear(vtx_t v, Flag& time_change)
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
	}
}





void Event_graph::get_cycle_path(vector<Vertex_edge>& ret, vtx_t start)
{
	ret.clear();

	vtx_t v = start;
	do {
		auto pred = this->cycle_pred[v];
		ret.push_back(pred);
		v = pred.v;
		assert(v < this->n_vtx);
	} while(v != start);
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