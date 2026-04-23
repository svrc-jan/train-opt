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
	enum Sync_state
	{
		NO_CHANGES,
		TIME_UPDATE,
		CYCLE_FOUND
	};

	struct Edge;
	struct Ordering;
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
	std::vector<vtx_t> cycle_found_vtx = {};
	
	Event_graph(const size_t n_vtx=0);
	~Event_graph() {}

	void set_n_vtx(size_t n_vtx);

	bool set_time_lb(vtx_t v, tim_t lb);

	void add_edge(const Edge& e);
	bool remove_edge(const Edge& e);
	bool update_edge(const Edge& e_old, const Edge& e_new);
	bool update_edge_same_vtx(const Edge& e_old, const Edge& e_new);

	void add_edges(const std::vector<Edge>& edges);
	void clear_edges();

	void remove_last_edges(const std::vector<Edge>& edges);
	void remove_last_edges_small(const std::vector<Edge>& edges);

	void set_all_edge_idx(edg_t idx);

	Sync_state sync(Flag& time_change);

	void get_cycle_path(std::vector<Vertex_edge>& ret, vtx_t target);
	void get_critical_path(std::vector<Vertex_edge>& ret, vtx_t end);
	
private:
	struct Edge_entry;
	
	vtx_t n_vtx = 0;
	std::vector<std::vector<Edge_entry>> edges_out = {};
	std::vector<std::vector<Edge_entry>> edges_in = {};

	uint8_t need_sync = false;

	Flag vtx_dirty;
	Flag visited;
	Flag rec_stack;

	std::vector<tim_t> time_lb = {};

	std::vector<vtx_t> need_list = {};
	std::vector<vtx_t> update_stack = {};

	std::vector<Vertex_edge> cycle_pred = {};
	std::vector<Vertex_edge> time_pred = {};

	std::queue<vtx_t> queue_;

	bool update_dfs(vtx_t v);
	void update_cycle_paths();
	bool update_time_clear(vtx_t v, Flag& time_change);


	Edge_entry* find_entry(std::vector<Edge_entry>& list, const Edge_entry& entry);
};


struct Event_graph::Edge_entry
{
	vtx_t v = VTX_MAX;
	dur_t d = 0;
	edg_t e = EDG_MAX;

	bool operator==(const Edge_entry& x) const
	{ return (v == x.v) && (d == x.d) && (e == x.e); }
};


struct Event_graph::Edge
{
	Interval<vtx_t> v = {VTX_MAX, VTX_MAX};
	dur_t d = 0;
	edg_t e = EDG_MAX;

	inline bool operator<(const Edge& x) const 
	{ return LEX_LT3(v.start, v.end, d, x.v.start, x.v.end, x.d); }

	inline bool operator==(const Edge& x) const
	{ return (v == x.v) && (d == x.d) && (e == x.e); }

	inline bool operator!=(const Edge& x) const
	{ return !(*this == x); }

	inline Edge_entry to_in() const { return {v.start, d, e}; }
	inline Edge_entry to_out() const { return {v.end, d, e}; }
	inline bool is_valid() const { return (v.start < VTX_MAX) && (v.end < VTX_MAX); }
};


struct Event_graph::Vertex_edge
{
	vtx_t v = VTX_MAX;
	edg_t e = EDG_MAX;

	Vertex_edge() : v(VTX_MAX), e(EDG_MAX) {}
	Vertex_edge(vtx_t v, edg_t e) : v(v), e(e) {}
	Vertex_edge(const Edge_entry& x) : v(x.v), e(x.e) {}
};


struct Event_graph::Ordering
{
	vtx_t from = VTX_MAX;
	vtx_t to = VTX_MAX;
	dur_t dur = 0;

	Edge to_edge(edg_t idx=EDG_MAX) const
	{ return Edge({from, to}, dur, idx); }

	inline bool operator==(const Ordering& x) const
	{ return (from == x.from) && (to == x.to) && (dur == x.dur); }

	inline bool operator!=(const Ordering& x) const
	{ return !(*this == x); }
};
