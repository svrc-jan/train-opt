#pragma once

#include <cstdint>
#include <vector>
#include <queue>
#include "utils/array.hpp"

#define BLOCK_SIZE 64

class Graph
{
public:
	struct Edge;
	struct Path_entry;
	
	Graph();
	~Graph();

	void set_vertices(uint16_t num);
	uint32_t add_edge(uint16_t v_from, uint16_t v_to, uint16_t dur);
	
	inline Array<Edge> edges_out(uint16_t v);
	inline Array<Edge> edges_in(uint16_t v);

	inline uint16_t deg_out(uint16_t v);
	inline uint16_t deg_in(uint16_t v);

	const std::vector<Path_entry>& make_path(
		const std::vector<uint16_t>& targets, const uint8_t* edge_valid=nullptr);
	inline const std::vector<Path_entry>& get_path() const { return this->path; }

	std::vector<uint32_t> lower_bound = {};

private:
	
	struct Idx_size;
	struct Block;

	uint16_t next_edge_idx = 0;
	
	std::vector<Block> block_out_vec = {};
	std::vector<Block> block_in_vec = {};

	std::vector<Path_entry> path = {};
	const uint8_t* _edge_valid = nullptr;

	inline Block& block_out(uint16_t v) { return this->block_out_vec[v/BLOCK_SIZE]; }
	inline Block& block_in(uint16_t v) { return this->block_in_vec[v/BLOCK_SIZE]; }

	void clear_blocks();

	inline void clear_path();

	void path_rec(uint16_t v);

};


struct Graph::Edge
{
	uint32_t idx = UINT32_MAX;
	uint16_t vertex = UINT16_MAX;
	uint16_t dur = 0;
};


struct Graph::Path_entry
{
	uint32_t time = 0;
	uint32_t edge = UINT32_MAX;
	uint16_t pred = UINT16_MAX;
	uint8_t done = 0;
};


struct Graph::Idx_size
{
	uint16_t idx = 0;
	uint16_t size = 0;
};


struct Graph::Block
{
	Idx_size range[BLOCK_SIZE];
	std::vector<Edge> edges = {};

	Block();
	~Block() {}

	void clear();

	uint16_t add_edge(uint16_t v, const Edge& edge);
	void remove_last_edges(uint16_t v, uint16_t n);

	inline Array<Edge> get_edges(uint16_t v);
	inline uint16_t deg(uint16_t v) { return this->range[v % BLOCK_SIZE].size; }
};


inline Array<Graph::Edge> Graph::edges_out(uint16_t v)
{
	return this->block_out(v).get_edges(v);
}

inline Array<Graph::Edge> Graph::edges_in(uint16_t v)
{
	return this->block_in(v).get_edges(v);
}

inline uint16_t Graph::deg_out(uint16_t v)
{
	return this->block_out(v).deg(v);
}

inline uint16_t Graph::deg_in(uint16_t v)
{
	return this->block_in(v).deg(v);
}


inline void Graph::clear_path()
{
	for (auto& x : this->path) { 
		x.done = 0;
	}
}


inline Array<Graph::Edge> Graph::Block::get_edges(uint16_t v)
{
	auto& r = this->range[v % BLOCK_SIZE];
	return { &(this->edges[r.idx]), r.size };
}
