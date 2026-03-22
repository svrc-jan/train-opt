#pragma once

#include "utils/array.hpp"
#include "instance.hpp"
#include "graph.hpp"

class Path_and_cycle
{
public:
	const Instance& inst;
	
	Path_and_cycle(const Instance& isnt);
	~Path_and_cycle();

	void set_paths(const Instance::Paths& paths);
	void solve();

private:
	struct Edge;
	struct Switch;
	struct Vtx_time;

	Graph graph;
	Instance::Paths paths;

	std::vector<Edge> op2edge = {};
	std::vector<Switch> vtx2sw = {};
	std::vector<Vtx_time> vtx_order = {};

	std::vector<idx_t> v_start = {};
	std::vector<idx_t> v_last = {};

	std::vector<tim_t> time_lb = {};
	std::vector<tim_t> time = {};

	void make_vtx_order();

	void merge_sort_vtx_order(idx_t t_left, idx_t t_right);
	void merge_vtx_order(idx_t t_left, idx_t t_mid, idx_t t_right);
};


struct Path_and_cycle::Edge
{
	edge_t idx = EDGE_MAX;
	vertex_t start = VERTEX_MAX;
	vertex_t end = VERTEX_MAX;
};

struct Path_and_cycle::Switch
{
	idx_t op_unlock = IDX_MAX;
	idx_t op_lock = IDX_MAX;
};


struct Path_and_cycle::Vtx_time
{
	idx_t vertex = IDX_MAX;
	idx_t train = IDX_MAX;
	tim_t time = TIME_MAX;
};
