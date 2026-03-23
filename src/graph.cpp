#include "graph.hpp"

#include <assert.h>

using namespace std;

Graph::Graph()
{

}


Graph::~Graph()
{
	
}


void Graph::set_vertices(uint16_t num)
{
	this->edges_in.resize(num, vector<Edge>(0));
	this->edges_out.resize(num, vector<Edge>(0));

	this->path.resize(num);
}


edge_t Graph::add_edge(vertex_t v_from, vertex_t v_to, gdur_t dur)
{
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

const std::vector<Graph::Path_entry>& Graph::make_path(const vector<vertex_t>& targets,
	 const std::vector<gtime_t>& lower_bound, const uint8_t* edge_valid)
{
	this->clear_path();
	this->_edge_valid = edge_valid;

	for (vertex_t v : targets) {
		this->path_rec(v, lower_bound);
	}

	this->_lower_bound = nullptr;
	this->_edge_valid = nullptr;

	return this->path;
}

void Graph::clear_path()
{
	for (auto& x : this->path) {
		x.done = 0;
	}
}


void Graph::path_rec(vertex_t v, const std::vector<gtime_t>& lower_bound)
{
	if (this->path[v].done) {
		return;
	}

	gtime_t new_time = lower_bound[v];
	this->path[v] = {new_time, EDGE_MAX, VERTEX_MAX, 1};

	for (auto& edge : this->edges_in[v]) {
		if (this->_edge_valid == nullptr || this->_edge_valid[edge.idx]) {
			this->path_rec(edge.vertex, lower_bound);
			new_time = this->path[edge.vertex].time + edge.dur;

			if (new_time > this->path[v].time) {
				this->path[v] = {new_time, edge.idx, edge.vertex, 1};
			}
		}
	}
}





