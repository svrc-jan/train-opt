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


enum Vertex_state
{
	STATE_WAIT,
	STATE_DONE,
	STATE_ON_STACK
};


class Graph
{
public:
	struct Edge;
	struct Search_entry;
	
	Graph() {}
	~Graph() {}

	void set_vertices(const vertex_t num);
	void clear_all();
	uint32_t add_edge(const vertex_t v_from, const vertex_t v_to, const gdur_t dur);

	inline size_t deg_out(vertex_t v) const { return this->edges_out[v].size(); } 
	inline size_t deg_in(vertex_t v) const { return this->edges_in[v].size(); }

	vertex_t make_order(const std::vector<vertex_t>& v_start, const uint8_t* edge_valid=nullptr);
	void make_time(const gtime_t* lower_bound, const uint8_t* edge_valid=nullptr);

	inline const std::vector<Search_entry>& get_search() const { return this->search; }


private:
	vertex_t n_vertex = 0;

	uint32_t next_edge_idx = 0;
	std::vector<edge_t> free_edge_idx = {};

	std::vector<std::vector<Edge>> edges_in = {};
	std::vector<std::vector<Edge>> edges_out = {};

	std::vector<Search_entry> search = {};
	std::vector<gtime_t> time = {};
	std::vector<vertex_t> order_idx = {};
	std::vector<vertex_t> order = {};
	std::vector<vertex_t> stack = {};

	edge_t get_free_edge_idx();
	
	void clear_search();
	void clear_time();
	vertex_t order_rec(const vertex_t v, const uint8_t* edge_valid);
};


struct Graph::Edge
{
	edge_t idx = EDGE_MAX;
	vertex_t vertex = VERTEX_MAX;
	gdur_t dur = 0;
};


struct Graph::Search_entry
{
	uint8_t state = STATE_WAIT;
	uint8_t dirty = 0;
	vertex_t pred = VERTEX_MAX;
	edge_t edge = EDGE_MAX;
};
