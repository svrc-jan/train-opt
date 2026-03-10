#pragma once

#include <vector>
#include <deque>
#include "utils/flag.hpp"
#include "time_type.hpp"
#include "preprocess.hpp"
#include "instance.hpp"

class Graph
{
public:
	struct Edge;
	struct Time_change;

	const Instance& inst;
	const Preprocess& prepr;

	Graph(const Preprocess& prepr);

	void set_num_vertices(const int n_vtx);
	void clear_constrains();

	void set_all_paths(const std::vector<std::vector<int>>& paths);
	void set_path(const std::vector<int>& path);

	bool add_edge(const Edge& edge);
	bool remove_edge(const Edge& edge);
	inline void restore_time_changes(const std::vector<Time_change> &changes);

	inline TIME_T get_time(const int v);
	inline TIME_T get_time_visited(const int v);

private:
	struct Constraint;
	struct Vertex_forward;
	struct Vertex_backward;
	struct Vertex_time_bounds;

	int n_ver = 0;

	std::vector<Vertex_forward> forward = {};
	std::vector<Vertex_backward> backward = {};
	std::vector<Vertex_time_bounds> time_bounds = {};

	std::vector<TIME_T> time = {};

	// aux
	std::deque<int> dq;

	Flag dirty;
	Flag visited;
	std::vector<int> need_update = {};

	void set_path_op(const int op);

	bool find_visited(const int v_begin, const int v_cycle);
	void mark_dirty(const int v_begin);
	void update_time(const int v_begin);
	
	bool find_visited_rec(const int v, const int v_cycle);
	void mark_dirty_rec(const int v);
	void update_time_rec(const int v);
	void update_time_visited_rec(const int v);
};


struct Graph::Edge
{
	int vertex_from = -1;
	int vertex_to = -1;
	TIME_T time = 0;
};


struct Graph::Constraint
{
	int vertex = -1;
	TIME_T time = 0;
};

struct Graph::Time_change
{
	int vertex = -1;
	TIME_T old_value = 0;
};


struct Graph::Vertex_forward
{
	int path = -1;
	std::vector<int> constrains = {};
};


struct Graph::Vertex_backward
{
	Constraint path = {-1, 0};
	std::vector<Constraint> constrains = {};
};


struct Graph::Vertex_time_bounds
{
	int lower = 0;
	int upper = INT_MAX;
};


TIME_T Graph::get_time(const int v)
{
	this->update_time_rec(v);
	return this->time[v];
}


TIME_T Graph::get_time_visited(const int v)
{
	this->visited.clear();
	this->update_time_visited_rec(v);
	
	return this->time[v];
}

