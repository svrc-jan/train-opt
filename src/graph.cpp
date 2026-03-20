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
	size_t num_blocks = (num + BLOCK_SIZE - 1)/BLOCK_SIZE;

	this->block_in_vec.resize(num_blocks, Block());
	this->block_out_vec.resize(num_blocks, Block());

	this->clear_blocks();

	this->path.resize(num);
}


void Graph::clear_blocks()
{
	for (auto& block : this->block_in_vec) {
		block.clear();
	}

	for (auto& block : this->block_out_vec) {
		block.clear();
	}
}


uint32_t Graph::add_edge(uint16_t v_from, uint16_t v_to, uint16_t dur)
{
	assert(this->next_edge_idx < UINT32_MAX);

	uint32_t idx_in = this->block_in(v_to).add_edge(v_to, {this->next_edge_idx, v_from, dur});
	uint32_t idx_out = this->block_out(v_from).add_edge(v_from, {this->next_edge_idx, v_to, dur});

	assert(idx_in == idx_out);

	if (idx_in == this->next_edge_idx) {
		this->next_edge_idx++;
	}

	return idx_in;
}


const std::vector<Graph::Path_entry>& Graph::make_path(
	const vector<uint16_t>& targets, const uint8_t* edge_valid)
{
	this->clear_path();
	this->_edge_valid = edge_valid;

	for (uint16_t v : targets) {
		this->path_rec(v);
	}
	this->_edge_valid = nullptr;

	return this->path;
}


void Graph::path_rec(uint16_t v)
{
	if (this->path[v].done) {
		return;
	}

	this->path[v] = {
		.time = this->lower_bound[v],
		.edge = UINT32_MAX,
		.pred = UINT16_MAX,
		.done = 1
	};

	for (auto& edge : this->edges_in(v)) {
		if (this->_edge_valid == nullptr || this->_edge_valid[edge.idx]) {
			this->path_rec(edge.vertex);
			uint32_t new_time = this->path[edge.vertex].time + edge.dur;

			if (new_time > this->path[v].time) {
				this->path[v] = {
					.time = new_time,
					.edge = edge.idx,
					.pred = edge.vertex,
					.done = 1
				};
			}
		}
	}
}


Graph::Block::Block()
{
	this->clear();
}


void Graph::Block::clear()
{
	for (size_t i = 0; i < BLOCK_SIZE; i++) {
		this->range[i] = {0, 0};
	}
	this->edges.clear();
}


uint16_t Graph::Block::add_edge(uint16_t v, const Edge& edge)
{
	uint16_t i = v % BLOCK_SIZE;
	uint16_t k = this->range[i].idx + this->range[i].size;

	uint16_t edge_idx = UINT16_MAX;

	auto& curr = this->edges[k];
	if (k < this->edges.size() && curr.vertex == UINT16_MAX) {
		edge_idx = curr.idx;

		curr.vertex = edge.vertex;
		curr.dur = edge.dur;
	}
	else {
		this->edges.insert(this->edges.begin() + k, edge);
		edge_idx = edge.idx;

		for (uint16_t j = i + 1; j < BLOCK_SIZE; j++) {
			this->range[j].idx++;
		}
	}

	this->range[i].size++;
	
	return edge_idx;
}


void Graph::Block::remove_last_edges(uint16_t v, uint16_t n)
{
	auto& r = this->range[v % BLOCK_SIZE];

	uint16_t new_size = r.size - n;
	while (r.size > new_size) {
		r.size--;
		this->edges[r.size].vertex = UINT16_MAX;
	}
}





