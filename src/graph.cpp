#include "graph.hpp"

#include <assert.h>

using namespace std;


void Graph::set_vertices(const vertex_t num)
{
	assert(num < VERTEX_MAX && num > 0);
	this->n_vertex = num;

	this->edges_in.resize(num, vector<Edge>(0));
	this->edges_out.resize(num, vector<Edge>(0));
	
	this->search.resize(num);
	this->time.resize(num);
	this->order_idx.resize(num);

	this->order.reserve(num);
	this->stack.reserve(num);
}


void Graph::clear_all()
{
	for (auto& edges : this->edges_in) {
		edges.clear();
	}

	for (auto& edges : this->edges_out) {
		edges.clear();
	}

	this->next_edge_idx = 0;
	this->free_edge_idx.clear();
}


edge_t Graph::add_edge(const vertex_t v_from, const vertex_t v_to, const gdur_t dur)
{
	assert(v_from != v_to && v_from < this->n_vertex && v_to < this->n_vertex);
	edge_t edge_idx = this->get_free_edge_idx();

	this->edges_in[v_to].push_back({edge_idx, v_from, dur});
	this->edges_out[v_from].push_back({edge_idx, v_from, dur});

	return edge_idx;
}


edge_t Graph::get_free_edge_idx()
{
	edge_t edge_idx;

	if (this->free_edge_idx.empty()) {
		assert(this->next_edge_idx < EDGE_MAX);
		edge_idx = this->next_edge_idx++;
	}
	else {
		edge_idx = this->free_edge_idx.back();
		this->free_edge_idx.pop_back();
	}

	return edge_idx;
}


/* Topological sort with cycle detection
 */

vertex_t Graph::make_order(const vector<vertex_t>& v_start, const uint8_t* edge_valid)
{
	this->clear_search();
	vertex_t ret;

	this->stack.clear();

	for (vertex_t v : v_start) {
		ret = this->order_rec(v, edge_valid);
		if (ret < VERTEX_MAX) {
			return ret;
		}
	}

	this->order.clear();
	while (!this->stack.empty()) {
		this->order.push_back(this->stack.back());
		this->stack.pop_back();
	}

	return VERTEX_MAX;
}

void Graph::clear_search()
{
	for (auto& x : this->search) {
		x.state = STATE_WAIT;
	}
}


vertex_t Graph::order_rec(const vertex_t v, const uint8_t* edge_valid)
{
	if (this->search[v].state == STATE_ON_STACK) {
		return v;
	}
	
	if (this->search[v].state == STATE_DONE) {
		return VERTEX_MAX;
	}

	this->search[v].state = STATE_ON_STACK;

	vertex_t ret;
	for (auto& edge : this->edges_out[v]) {
		if (edge_valid == nullptr || edge_valid[edge.idx]) {
			vertex_t w = edge.vertex;
			this->search[w].pred = v;
			this->search[w].edge = edge.idx;
			
			ret = order_rec(w, edge_valid);
			
			if (ret < VERTEX_MAX) {
				break;
			}
		}
	}

	this->stack.push_back(v);
	this->search[v].state = STATE_DONE;

	return ret;
}


void Graph::make_time(const gtime_t* lower_bound, const uint8_t* edge_valid)
{
	this->clear_time();

	vertex_t order_size = this->order.size();
	
	for (vertex_t idx = 0; idx < order_size; idx++) {
		vertex_t v = this->order[idx];

		this->order_idx[v] = idx;
		this->time[v] = (lower_bound == nullptr) ? 0 : lower_bound[v];
		
		for (auto& edge : this->edges_in[v]) {
			vertex_t u = edge.vertex;
			if (edge_valid == nullptr || edge_valid[u]) {
				assert(this->search[edge.vertex].state == STATE_DONE);
			
			}

			gtime_t edge_time = this->time[v];
			if (this->time[v] < edge_time) {
				this->time[v] = edge_time;
				this->search[v].pred = u;
				this->search[v].edge = edge.idx;
			}
		}

		this->search[v].state = STATE_DONE;
	}
}


void Graph::clear_time()
{
	for (auto& x : this->search) {
		x = {STATE_WAIT, 0, VERTEX_MAX, EDGE_MAX};
	}
}

