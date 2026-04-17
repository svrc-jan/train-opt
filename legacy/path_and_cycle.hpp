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
	struct Edge_idx;
	struct Edge_dur;
	struct Switch;
	struct Vtx_time;

	struct Res_use;
	struct Res_lock;
	struct Res_col;

	Graph graph;
	Instance::Paths paths;

	std::vector<Edge_idx> op2edge = {};
	std::vector<Switch> vtx2sw = {};
	std::vector<Vtx_time> vtx_order = {};

	std::vector<idx_t> v_start = {};
	std::vector<idx_t> v_last = {};

	std::vector<tim_t> time_lb = {};

	std::vector<std::vector<Instance::Res>> res_locks;
	std::vector<Edge_dur> res_col_edges;

	void make_vtx_order();
	void merge_sort_vtx_order(size_t t_left, size_t t_right);
	void merge_vtx_order(size_t t_left, size_t t_mid, size_t t_right);
	void verify_vtx_order();

	bool find_res_col(Res_col& res_col);
	void make_res_col_edges(const Res_col& res_col);
	void fill_res_locks(size_t v_start, size_t v_end);
};


struct Path_and_cycle::Edge_idx
{
	edge_t idx = EDGE_MAX;
	vertex_t start = VERTEX_MAX;
	vertex_t end = VERTEX_MAX;
};

struct Path_and_cycle::Edge_dur
{
	vertex_t start = VERTEX_MAX;
	vertex_t end = VERTEX_MAX;
	dur_t dur = DUR_MAX;
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


struct Path_and_cycle::Res_use
{
	idx_t op = IDX_MAX;
	idx_t train = IDX_MAX;
	dur_t res_time = 0;
};


struct Path_and_cycle::Res_col
{
	idx_t v1 = IDX_MAX;
	idx_t t1 = IDX_MAX;
	idx_t v2 = IDX_MAX;
	idx_t t2 = IDX_MAX;

	void swap()
	{
		std::swap(v1, v2);
		std::swap(t1, t2);
	}
};
