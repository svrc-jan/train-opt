#pragma once

#include <vector>
#include <queue>

#include "time_type.hpp"
#include "graph.hpp"


class Schedule
{
public:
	using Res_edges = std::pair<Graph::Edge, Graph::Edge>;

	const Instance& inst;
	const Preprocess& prepr;
	Graph& graph;

	Schedule(Graph& graph);

	void set_all_paths(const std::vector<std::vector<int>>& paths);
	void set_path(const int train_idx, const std::vector<int>& path);
	bool process_from_start(Res_edges& res_edges);

private:
	struct Idx;
	struct Event;
	struct Res_use;

	int n_trains = 0;
	int n_res = 0;

	std::vector<std::vector<int>> paths = {};
	// std::vector<int> path_idx = {};

	void make_res_edges(Res_edges& res_edges, const int res_idx, 
		const Res_use& res_use1, const Res_use& res_use2);

	inline int path_size(const int train_idx) const { return this->paths[train_idx].size(); }
	inline int get_op(const Idx& idx);
	inline TIME_T get_time_start(const Idx& idx);
	inline TIME_T get_time_end(const Idx& idx);
	inline TIME_T get_time_unlock(const Res_use& res_use);

	int get_level_lock(const int res_idx, Idx idx);
	int get_level_unlock(const int res_idx, Idx idx, int& res_time);

	// aux
	std::priority_queue<Event> prio_queue;
};


struct Schedule::Idx
{
	int train = -1;
	int path = -1;

	Idx pred() const 
	{ return {.train = this->train, .path = this->path - 1}; };

	Idx succ() const 
	{ return {.train = this->train, .path = this->path + 1}; };
};

struct Schedule::Event
{
	Idx idx = Idx();
	TIME_T time = 0;

	bool operator<(const Event& other) const
	{ return this->time > other.time; }
};


struct Schedule::Res_use
{
	Idx idx = Idx();
	TIME_T res_time = 0;
};


int Schedule::get_op(const Idx& idx)
{
	const auto& path = this->paths[idx.train];
	return (idx.path >= 0 && idx.path < (int)path.size()) ? path[idx.path] : -1;
}


TIME_T Schedule::get_time_start(const Idx& idx)
{
	int level = this->prepr.op_level_start(this->get_op(idx));
	return this->graph.get_time(level);
}


TIME_T Schedule::get_time_end(const Idx& idx)
{
	int level = this->prepr.op_level_end(this->get_op(idx));
	return this->graph.get_time(level);
}


TIME_T Schedule::get_time_unlock(const Res_use& res_use)
{
	return this->get_time_end(res_use.idx) + res_use.res_time;
}