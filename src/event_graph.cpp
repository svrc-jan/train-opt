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

	this->update_stack.reserve(n_vtx);

	this->cycle_pred.resize(n_vtx);
	this->shortest_cycle.resize(n_vtx);

	this->time_lb.resize(n_vtx, 0);
	this->time.resize(n_vtx);
	this->time_pred.resize(n_vtx);
}


void Event_graph::add_edge(const Edge& x)
{
	assert(x.v.start < this->n_vtx && x.v.end < this->n_vtx);

	this->edges_in[x.v.end].push_back(x.to_in());
	this->edges_out[x.v.start].push_back(x.to_out());

	this->need_update += x.v.end;
}


bool Event_graph::remove_edge(const Edge& x)
{
	auto& edg_in = this->edges_in[x.v.end];
	auto in_entry = find(edg_in.rbegin(), edg_in.rend(), x.to_in());
	
	auto& edg_out = this->edges_out[x.v.start];
	auto out_entry = find(edg_out.rbegin(), edg_out.rend(), x.to_out());

	bool in_miss = in_entry == edg_in.rend();
	bool out_miss = out_entry == edg_out.rend();
	if (in_miss || out_miss) {
		assert(in_miss && out_miss);
		return false;
	}

	*in_entry = edg_in.back();
	edg_in.pop_back();

	*out_entry = edg_out.back();
	edg_out.pop_back();

	this->need_update += x.v.end;

	return true;
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
			this->need_update += v;
		}

		cnt = out_count[v];
		if (cnt > 0) {
			auto& x = this->edges_out[v];
			x.resize(x.size() - cnt);
			this->need_update += v;
		}
	}
}

void Event_graph::remove_last_edges_small(const std::vector<Edge>& edges)
{
	for (auto& x : edges) {
		this->edges_in[x.v.end].pop_back();
		this->edges_out[x.v.start].pop_back();

		this->need_update += x.v.start;
		this->need_update += x.v.end;
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
	this->update_stack.clear();
	this->cycle_vtx = VTX_MAX;

	for (vtx_t v : need_list) {
		if (this->get_cycle_rec(v)) {
			return true;
		}
	}

	for (vtx_t v : this->update_stack | views::reverse) {
		this->update_time(v);
	}

	return false;
}


bool Event_graph::get_cycle_rec(vtx_t v)
{
	if (this->rec_stack[v]) {
		this->cycle_vtx = v;
		return true;
	}

	if (this->visited[v]) {
		return false;
	}

	this->visited += v;
	this->rec_stack += v;

	for (const auto& x : this->edges_out[v]) {
		if(get_cycle_rec(x.v)) {
			return true;
		}
	}

	this->rec_stack -= v;
	this->update_stack.push_back(v);

	return false;
}



void Event_graph::update_time(vtx_t v)
{
	assert(this->visited[v]);

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
}


const vector<Event_graph::Vertex_edge>& Event_graph::get_shortest_cycle(vtx_t start)
{
	vector<Vertex_edge>& ret = this->shortest_cycle;
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
				this->cycle_pred[x.v] = {v, x.e};
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
		assert(v < this->n_vtx);
	} while(v != start);

	return ret;
}

const vector<Event_graph::Vertex_edge>& Event_graph::get_critical_path(vtx_t start)
{
	vector<Vertex_edge>& ret = this->critical_path;
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

	return ret;
}