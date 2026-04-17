#include "graph.hpp"

#include <cassert>
#include <iostream>

using namespace std;


Graph::Graph(const size_t n_vtx)
{
	this->set_n_vtx(n_vtx);
}


void Graph::set_n_vtx(const size_t n_vtx)
{
	assert(n_vtx < VTX_MAX);
	this->n_vtx = n_vtx;

	this->edges.resize(n_vtx, {});

	this->visited.set_n_items(n_vtx);
	this->rec_stack.set_n_items(n_vtx);

	this->order.reserve(n_vtx);

	this->time_lb.resize(n_vtx, 0);
	this->time.resize(n_vtx);
}


void Graph::add_edge(const Edge& e)
{
	this->edges[e.v_from].push_back(e);
}


bool Graph::remove_edge(const Edge& e)
{
	auto entry = this->get_edge_entry(e);
	
	if (entry == nullptr) {
		return true;
	}

	auto& edges_from = this->edges[e.v_from];
	
	*entry = edges_from.back();
	edges_from.pop_back();

	return false;
}


bool Graph::update_edge(const Edge& e, size_t idx)
{
	auto& edges_from = this->edges[e.v_from];
	if (edges_from.size() <= idx) {
		this->add_edge(e);
		return true;
	}

	edges_from[idx] = e;
	return false;
}


bool Graph::update_edge(const Edge& e_old, const Edge& e_new)
{
	bool ret = false;
	if (e_old.v_from == e_new.v_from) {
		auto entry = this->get_edge_entry(e_old);
		if (entry == nullptr) {
			this->add_edge(e_new);
			ret = true;
		}
		else{
			*entry = e_new;
		}
	}
	else {
		ret = this->remove_edge(e_old);
		this->add_edge(e_new);
	}
	
	return ret;
}


void Graph::clear_edges()
{
	for (auto& x : this->edges) {
		x.clear();
	}
}


Graph::Edge_entry* Graph::get_edge_entry(const Edge& e)
{
	if (e.v_from == VTX_MAX || e.v_from == VTX_MAX) {
		return nullptr;
	}

	for (auto& entry : this->edges[e.v_from] | views::reverse) {
		if (entry.v == e.v_to) {
			return &entry;
		}
	}

	return nullptr;
}


bool Graph::has_cycle(const std::vector<vtx_t>& start_vtx)
{
	this->visited.clear();
	this->rec_stack.clear();

	assert(this->visited.get_true_count() == 0);
	assert(this->rec_stack.get_true_count() == 0);

	for (vtx_t v : start_vtx) {
		if (this->has_cycle_rec(v)) {
			return true;
		}
	}

	return false;
}


bool Graph::has_cycle_rec(vtx_t v)
{
	if (this->rec_stack[v]) {
		return true;
	}

	if (this->visited[v]) {
		return false;
	}

	this->visited += v;
	this->rec_stack += v;
	assert(this->visited[v] && this->rec_stack[v]);

	for (const auto& e : this->edges[v]) {
		if (has_cycle_rec(e.v)) {
			return true;
		}
	}

	this->rec_stack -= v;
	assert(!this->rec_stack[v]);

	return false;
}


bool Graph::make_order(const std::vector<vtx_t>& start_vtx)
{
	this->visited.clear();
	this->rec_stack.clear();

	size_t stack_size = 0;
	vtx_t stack[this->n_vtx];

	for (vtx_t v : start_vtx) {
		if (this->make_order_rec(v, stack, stack_size)) {
			return true;
		}
	}

	this->order.clear();
	while (stack_size > 0) {
		this->order.push_back(stack[--stack_size]);
	}

	return false;
}


bool Graph::make_order_rec(vtx_t v, vtx_t* stack, size_t& stack_size)
{
	if (this->rec_stack[v]) {
		return true;
	}

	if (this->visited[v]) {
		return false;
	}

	this->visited += v;
	this->rec_stack += v;

	for (const auto& e : this->edges[v] | views::reverse) {
		if (make_order_rec(e.v, stack, stack_size)) {
			return true;
		}
	}

	stack[stack_size++] = v;
	this->rec_stack -= v;

	return false;
}


void Graph::update_time()
{
	for (auto v : this->order) {
		this->time[v] = this->time_lb[v];
	}

	for (auto v : this->order) {
		for (auto& e : this->edges[v]) {
			this->time[e.v] = MAX(this->time[e.v], this->time[v] + e.d);
		}
	}
}

