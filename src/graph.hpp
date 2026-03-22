#pragma once

#include <cstdint>
#include <vector>
#include <queue>

typedef uint32_t edge_t;
typedef uint16_t vertex_t;
typedef uint32_t gtime_t;
typedef uint16_t gdur_t;

#define EDGE_MAX UINT32_MAX
#define VERTEX_MAX UINT16_MAX
#define GTIME_MAX UINT32_MAX
#define GDUR_MAX UINT16_MAX


class Graph
{
public:
	struct Edge;
	struct Path_entry;
	
	Graph();
	~Graph();

	void set_vertices(vertex_t num);
	uint32_t add_edge(vertex_t v_from, vertex_t v_to, gdur_t dur);

	inline size_t deg_out(vertex_t v) { return this->edges_out[v].size(); } 
	inline size_t deg_in(vertex_t v) { return this->edges_in[v].size(); }

	const std::vector<Path_entry>& make_path(const std::vector<vertex_t>& targets,
		const gtime_t* lower_bound=nullptr, const uint8_t* edge_valid=nullptr);

	inline const std::vector<Path_entry>& get_path() const { return this->path; }


private:
	struct Idx_size;
	struct Block;

	uint32_t next_edge_idx = 0;
	std::vector<edge_t> free_edge_idx = {};

	std::vector<std::vector<Edge>> edges_in;
	std::vector<std::vector<Edge>> edges_out;

	std::vector<Path_entry> path = {};
	const gtime_t* _lower_bound = nullptr;
	const uint8_t* _edge_valid = nullptr;

	edge_t get_free_edge_idx();
	
	void clear_path();
	void path_rec(vertex_t v);
};


struct Graph::Edge
{
	edge_t idx = EDGE_MAX;
	vertex_t vertex = VERTEX_MAX;
	gdur_t dur = 0;
};


struct Graph::Path_entry
{
	gtime_t time = 0;
	edge_t edge = EDGE_MAX;
	vertex_t pred = VERTEX_MAX;
	uint8_t done = 0;
};
