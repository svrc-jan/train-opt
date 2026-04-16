#pragma once

#include <vector>
#include <cstdint>
#include <limits>
#include <ranges>

#include "utils/flag.hpp"
#include "utils/lex_comp.hpp"

class Graph
{

public:
	struct Edge;

	typedef uint16_t vtx_t;
	typedef uint16_t dur_t;
	typedef uint32_t tim_t;

	static const vtx_t VTX_MAX = std::numeric_limits<vtx_t>::max();
	static const dur_t DUR_MAX = std::numeric_limits<dur_t>::max();
	static const tim_t TIM_MAX = std::numeric_limits<tim_t>::max();

	Graph(const size_t n_vtx=0);
	~Graph() {}

	void set_n_vtx(size_t n_vtx);

	void add_edge(const Edge& e);

	bool remove_edge(const Edge& e);
	bool update_edge(const Edge& e, size_t idx);
	bool update_edge(const Edge& e_old, const Edge& e_new);

	void clear_edges();
	
	bool has_cycle(const std::vector<vtx_t>& start_vtx);
	bool make_order(const std::vector<vtx_t>& start_vtx);
	
private:
	struct Edge_entry;
	
	vtx_t n_vtx = 0;
	std::vector<std::vector<Edge_entry>> edges = {};

	Flag visited;
	Flag rec_stack;

	std::vector<vtx_t> order = {};
	std::vector<tim_t> time_lb = {};
	std::vector<tim_t> time = {};

	Edge_entry* get_edge_entry(const Edge& e);

	bool has_cycle_rec(vtx_t v);
	bool make_order_rec(vtx_t v, vtx_t* stack, size_t& stack_size);
	std::vector<tim_t>& update_time();
};


struct Graph::Edge
{
	vtx_t v_from = VTX_MAX;
	vtx_t v_to = VTX_MAX;
	dur_t d = 0;

	inline bool operator<(const Edge& x) const 
	{ return LEX_LT3(v_from, v_to, d, x.v_from, x.v_to, x.d); }

	inline bool operator==(const Edge& x) const
	{ return LEX_EQ3(v_from, v_to, d, x.v_from, x.v_to, x.d); }

	inline bool operator!=(const Edge& x) const
	{ return !(*this == x); }
};


struct Graph::Edge_entry
{
	vtx_t v = VTX_MAX;
	dur_t d = 0;

	Edge_entry(const Edge& e) : v(e.v_to), d(e.d) {}
};
