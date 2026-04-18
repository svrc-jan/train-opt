#include "event_graph.hpp"

#include <cassert>
#include <iostream>

using namespace std;


Event_graph::Event_graph(const size_t n_vtx)
{
	this->set_n_vtx(n_vtx);
}


void Event_graph::set_n_vtx(const size_t n_vtx)
{
	assert(n_vtx < VTX_MAX);
	this->n_vtx = n_vtx;

	this->edges_in.resize(n_vtx, {});
	this->edges_out.resize(n_vtx, {});

	this->visited.set_n_items(n_vtx);
	this->rec_stack.set_n_items(n_vtx);
	this->need_update.set_n_items(n_vtx);

	this->cycle_pred.resize(n_vtx);
	this->shortest_cycle.resize(n_vtx);

	this->time_dirty.set_n_items(n_vtx);
	this->time_lb.resize(n_vtx, 0);
	this->_time.resize(n_vtx);
	this->time_pred.resize(n_vtx);
}


void Event_graph::add_edge(const Edge& x)
{
	this->edges_in[x.v.end].push_back({x.e, x.v.start, x.d});
	this->edges_out[x.v.start].push_back({x.e, x.v.end, x.d});

	this->need_update += x.v.end;
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
		}

		cnt = out_count[v];
		if (cnt > 0) {
			auto& x = this->edges_out[v];
			x.resize(x.size() - cnt);
		}
	}
}

void Event_graph::remove_last_edges_small(const std::vector<Edge>& edges)
{
	for (auto& x : edges) {
		this->edges_in[x.v.end].pop_back();
		this->edges_out[x.v.start].pop_back();
	}
}


bool Event_graph::update()
{
	auto need_list = this->need_update.get_true_list();

	if (need_list.empty()) {
		return false;
	}

	this->need_update.clear();

	this->visited.clear();
	this->rec_stack.clear();
	this->cycle_vtx = VTX_MAX;

	for (vtx_t v : need_list) {
		vtx_t ret = this->get_cycle_rec(v);
		if (ret < VTX_MAX) {
			this->cycle_vtx = ret;
			return true;
		}
	}

	this->time_dirty.set_true(this->visited);

	return false;
}


Event_graph::vtx_t Event_graph::get_cycle_rec(vtx_t v)
{
	if (this->rec_stack[v]) {
		return v;
	}

	if (this->visited[v]) {
		return VTX_MAX;
	}

	this->visited += v;
	this->rec_stack += v;

	for (const auto& x : this->edges_out[v]) {
		vtx_t ret = get_cycle_rec(x.v);
		if (ret < VTX_MAX) {
			return ret;
		}
	}

	this->rec_stack -= v;

	return VTX_MAX;
}

void Event_graph::update_time(vtx_t v)
{
	if (!this->time_dirty[v]) {
		return;
	}
	
	this->_time[v] = this->time_lb[v];
	this->time_pred[v] = Edge_vertex();

	for (auto& x : edges_in[v]) {
		this->update_time(x.v);
		tim_t new_time = this->_time[x.v] + x.d;
		if (this->_time[v] < new_time) {
			this->_time[v] = new_time;
			this->time_pred[v] = x;
		}
	}
}


const vector<Event_graph::Edge_vertex>& Event_graph::get_shortest_cycle(vtx_t start)
{
	vector<Edge_vertex>& ret = this->shortest_cycle;
	ret.clear();

	if (start == VTX_MAX) {
		return ret;
	}

	assert(this->que.empty());

	this->visited.clear();
	this->que.push(this->cycle_vtx);

	bool found = false;
	while (!found && !this->que.empty()) {
		vtx_t v = this->que.front(); this->que.pop();
		
		for (auto& x : this->edges_out[v]) {
			if (!visited[x.v]) {
				this->cycle_pred[x.v] = {x.e, v};
				visited += x.v;
			}
			if (x.v == start) {
				found = true;
				break;
			}
		}
	}

	assert(found);

	vtx_t v = start;
	do {
		auto pred = this->cycle_pred[v];
		ret.push_back(pred);
		v = pred.v;
	} while(v != start);

	return ret;
}

