#pragma once

#include <print>
#include <unordered_map>
#include "utils/hasher.hpp"
#include "schedule.hpp"


class Solver
{
public:
	const Instance& inst;
	const Preprocess& prepr;

	Graph& graph;
	Schedule& sched;

	Solver(Schedule& sched);
	

	bool solve_with_train_prio(const std::vector<double>& prio);

private:
	std::vector<int> level_train = {};

};