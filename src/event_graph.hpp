#pragma once

#include <vector>
#include <cstdint>
#include <queue>
#include <limits>
#include <ranges>

#include "utils/macros.hpp"
#include "utils/interval.hpp"
#include "utils/flag.hpp"
#include "utils/lex_comp.hpp"

class Event_graph
{

public:
	struct Edge;
	struct Vertex_edge;

	typedef uint16_t edg_t;
	typedef uint16_t vtx_t;
	typedef uint16_t dur_t;
	typedef uint32_t tim_t;

	static constexpr edg_t EDG_MAX = std::numeric_limits<edg_t>::max();
	static constexpr vtx_t VTX_MAX = std::numeric_limits<vtx_t>::max();
	static constexpr dur_t DUR_MAX = std::numeric_limits<dur_t>::max();
	static constexpr tim_t TIM_MAX = std::numeric_limits<tim_t>::max();

	std::vector<tim_t> time = {};
	std::vector<tim_t> time_lb = {};
	
	std::vector<Vertex_edge> shortest_cycle = {};
	std::vector<Vertex_edge> critical_path = {};

	Event_graph(const size_t n_vtx=0);
	~Event_graph() {}

	void set_n_vtx(size_t n_vtx);

	void add_edge(const Edge& e);
	bool remove_edge(const Edge& e);
	bool update_edge(const Edge& e_old, const Edge& e_new);

	void add_edges(const std::vector<Edge>& edges);
	void clear_edges();

	void remove_last_edges(const std::vector<Edge>& edges);
	void remove_last_edges_small(const std::vector<Edge>& edges);

	void set_all_edge_idx(edg_t idx);

	/* return true if cycle */
	bool update();
	const std::vector<Vertex_edge>& get_shortest_cycle(vtx_t start);
	const std::vector<Vertex_edge>& get_critical_path(vtx_t end);

	inline const std::vector<Vertex_edge>& get_shortest_cycle()
	{ return get_shortest_cycle(cycle_vtx); }
	
private:
	struct Edge_entry;
	
	vtx_t n_vtx = 0;
	std::vector<std::vector<Edge_entry>> edges_out = {};
	std::vector<std::vector<Edge_entry>> edges_in = {};

	Flag visited;
	Flag rec_stack;
	Flag need_update;

	std::vector<vtx_t> update_stack = {};

	vtx_t cycle_vtx;
	std::vector<Vertex_edge> cycle_pred = {};

	std::vector<Vertex_edge> time_pred = {};

	std::queue<vtx_t> que;


	bool get_cycle_rec(vtx_t v);
	void update_time(vtx_t v);
};


struct Event_graph::Edge
{
	Interval<vtx_t> v = {VTX_MAX, VTX_MAX};
	dur_t d = 0;
	edg_t e = EDG_MAX;

	inline bool operator<(const Edge& x) const 
	{ return LEX_LT3(v.start, v.end, d, x.v.start, x.v.end, x.d); }

	inline bool operator==(const Edge& x) const
	{ return (v.start == x.v.start) && (v.end == x.v.end) && (d == x.d) && (e == x.e); }

	inline bool operator!=(const Edge& x) const
	{ return !(*this == x); }

	inline Edge_entry to_in() const { return {v.end, d, e}; }
	inline Edge_entry to_out() const { return {v.start, d, e}; }
};


struct Event_graph::Edge_entry
{
	vtx_t v = VTX_MAX;
	dur_t d = 0;
	edg_t e = EDG_MAX;
};



struct Event_graph::Vertex_edge
{
	vtx_t v = VTX_MAX;
	edg_t e = EDG_MAX;

	Vertex_edge() : v(VTX_MAX), e(EDG_MAX) {}
	Vertex_edge(vtx_t v, edg_t e) : v(v), e(e) {}
	Vertex_edge(const Edge_entry& x) : v(x.v), e(x.e) {}
};