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
	struct Edge_vertex;

	typedef uint16_t edg_t;
	typedef uint16_t vtx_t;
	typedef uint16_t dur_t;
	typedef uint32_t tim_t;

	static constexpr edg_t EDG_MAX = std::numeric_limits<edg_t>::max();
	static constexpr vtx_t VTX_MAX = std::numeric_limits<vtx_t>::max();
	static constexpr dur_t DUR_MAX = std::numeric_limits<dur_t>::max();
	static constexpr tim_t TIM_MAX = std::numeric_limits<tim_t>::max();

	std::vector<tim_t> time_lb = {};
	
	std::vector<Edge_vertex> shortest_cycle = {};
	std::vector<Edge_vertex> critical_path = {};

	Event_graph(const size_t n_vtx=0);
	~Event_graph() {}

	void set_n_vtx(size_t n_vtx);

	void add_edge(const Edge& e);
	bool set_edge_by_idx(const Edge& e, size_t idx);
	void add_edges(const std::vector<Edge>& edges);
	
	void clear_edges();
	void remove_last_edges(const std::vector<Edge>& edges);
	void remove_last_edges_small(const std::vector<Edge>& edges);

	void set_all_edge_idx(edg_t idx);

	/* return true if cycle */
	bool update();
	const std::vector<Edge_vertex>& get_shortest_cycle(vtx_t start);
	const std::vector<Edge_vertex>& get_critical_path(vtx_t end);

	inline const std::vector<Edge_vertex>& get_shortest_cycle()
	{ return get_shortest_cycle(cycle_vtx); }

	inline tim_t time(vtx_t v);
	
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
	std::vector<Edge_vertex> cycle_pred = {};

	std::vector<tim_t> _time = {};
	std::vector<Edge_vertex> time_pred = {};

	std::queue<vtx_t> que;
	
	Edge_entry* get_edge_entry(const Edge& e);

	bool get_cycle_rec(vtx_t v);
	void update_time(vtx_t v);
};


struct Event_graph::Edge
{
	Interval<vtx_t> v;
	edg_t e = EDG_MAX;
	dur_t d = 0;

	inline bool operator<(const Edge& x) const 
	{ return LEX_LT3(v.start, v.end, d, x.v.start, x.v.end, x.d); }

	inline bool operator==(const Edge& x) const
	{ return LEX_EQ3(v.start, v.end, d, x.v.start, x.v.end, x.d); }

	inline bool operator!=(const Edge& x) const
	{ return !(*this == x); }
};


struct Event_graph::Edge_entry
{
	vtx_t v = VTX_MAX;
	edg_t e = EDG_MAX;
	dur_t d = 0;
};


struct Event_graph::Edge_vertex
{
	edg_t e = EDG_MAX;
	vtx_t v = VTX_MAX;

	Edge_vertex() : e(EDG_MAX), v(EDG_MAX) {}
	Edge_vertex(edg_t e, vtx_t v) : e(e), v(v) {}
	Edge_vertex(const Edge_entry& x) : e(x.e), v(x.v) {}
};

inline Event_graph::tim_t Event_graph::time(vtx_t v)
{
	return this->_time[v];
}