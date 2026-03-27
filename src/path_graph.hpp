#pragma once

#include "utils/interval.hpp"
#include "graph.hpp"
#include "instance.hpp"

class Path_graph
{
public:
	struct Alt_edges;
	const Instance& inst;

	Path_graph(const Instance& inst);

	void set_all_paths(const Instance::Paths& paths);
	void set_path(const idx_t t, const Array<idx_t>& path);
	std::vector<Alt_edges> group_edges(const idx_t t1, const idx_t t2);

private:
	struct Res_use;	

	Graph graph;
	Instance::Paths paths;

	std::vector<vertex_t> v_start = {};
	std::vector<vertex_t> v_end = {};

	std::vector<vertex_t> op_vtx = {};

	std::vector<idx_t> vtx_op_in = {};
	std::vector<idx_t> vtx_op_out = {};


	std::vector<std::vector<Res_use>> path_res_uses = {};
};


struct Path_graph::Res_use
{
	Interval<vertex_t> vtx = {VERTEX_MAX, VERTEX_MAX};
	idx_t idx = IDX_MAX;
	dur_t time = 0;

	static bool idx_comparator(const Res_use& a, const Res_use& b) {  return a.idx < b.idx; }
};


struct Path_graph::Alt_edges
{
	Interval<vertex_t> first = {VERTEX_MAX, VERTEX_MAX};
	Interval<vertex_t> second = {VERTEX_MAX, VERTEX_MAX};
};
