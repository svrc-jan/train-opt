#pragma once

#include "instance.hpp"
#include "preprocess.hpp"
#include "localize.hpp"
#include "graph.hpp"

class Solver
{
public:
	struct Res_use;

	const Instance& inst;
	const Preprocess& prepr;
	const Localize& local;

	typedef Instance::idx_t idx_t;
	typedef Instance::dur_t dur_t;
	typedef Instance::tim_t tim_t;
	typedef uint8_t cnt_t;

	static const idx_t IDX_MAX = Instance::IDX_MAX;
	static const dur_t DUR_MAX = Instance::DUR_MAX;
	static const tim_t TIM_MAX = Instance::TIM_MAX;

	
	Solver(const Localize& local);
	void get_init_sol();

	tim_t get_obj();

private:
	Graph graph;

	std::vector<Res_use*> res_use;
	std::vector<Res_use> res_use_data;

	std::vector<idx_t> path = {};
	std::vector<idx_t> first_levels = {};

	void init_res_use();
	void init_path();

	void update_res_use();

	void add_path_to_graph();
};


struct Solver::Res_use
{
	Interval<idx_t> level;
	dur_t time = 0;
};