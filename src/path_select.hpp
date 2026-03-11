#pragma once

#include "gurobi_c++.h"

#include "instance.hpp"
#include "preprocess.hpp"


class Path_select
{
public:
	const Instance& inst;
	const Preprocess& prepr;

	Path_select(const Preprocess& prepr);
	~Path_select();

	void select_paths(std::vector<std::vector<int>>& paths,
		std::vector<int>& junct_time);

	void select_paths(std::vector<std::vector<int>>& paths);

private:
	struct Interval;

	void propagate_junct_time(std::vector<int>& junct_time);
	std::vector<Interval> get_op_intervals(
		const std::vector<int>& junct_time);

	void select_train_path(std::vector<std::vector<int>>& paths, 
		const std::vector<int>& op_cost, int t);
};


struct Path_select::Interval
{
	int start;
	int end;

	bool operator<(const Interval& other)
	{
		return (this->start < other.start) ||
			((this->start == other.start) && (this->end < other.end));
	}
};
