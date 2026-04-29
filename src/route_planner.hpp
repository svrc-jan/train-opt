#pragma once

#include "gurobi_c++.h"

#include "utils/tracked.hpp"
#include "instance.hpp"
#include "preprocess.hpp"
#include "link_graph.hpp"
#include "chunk_manager.hpp"


#ifndef GBR_EXCEPTION
#define GBR_EXCEPTION 20
#endif


class Route_planner
{
public:
	struct Op;
	struct Level;
	struct Route;
	
	typedef Instance::idx_t idx_t;
	typedef Instance::dur_t dur_t;
	typedef Instance::tim_t tim_t;
	typedef Preprocess::idx_pr idx_pr;

	static constexpr idx_t IDX_MAX = Instance::IDX_MAX;
	static constexpr dur_t DUR_MAX = Instance::DUR_MAX;
	static constexpr tim_t TIM_MAX = Instance::TIM_MAX;

	const Instance& inst;
	const Preprocess& prepr;

	Batch<idx_t, int16_t> op_change;
	Batch<idx_pr, int16_t> op_succ_change;
	Batch<idx_t, tim_t> level_time_change;


	Route_planner(const Preprocess& prepr, Link_graph& link_graph, 
		Chunk_manager& chunk_mrng, GRBEnv& grb_env);
	~Route_planner();

	void make_init_routes();
	void optimize_routes();

private:
	struct Flow_cons;
	
	Link_graph& link_graph;
	Chunk_manager& chunk_mngr;
	GRBModel model;

	std::vector<Op> ops = {};
	std::vector<Level> levels = {};
	std::vector<Route> routes = {};

	std::vector<GRBConstr> flow_constr = {};

	std::vector<double> chunk_price = {};

	void get_random_routes();
	void update_route_ops();
	void update_level_time(idx_t t);

	void sync_extern();
	void get_op_changes();
	void get_time_changes();

	void find_req_routes();
	void add_route_vars();
	void add_flow_constr();

	bool optimize_model();

	void freeze_all();
	void unfreeze_all();

	void init_data();
	void init_ops();
	void init_levels();
	void init_routes();
	void init_chunks();
	void init_model();
};


struct Route_planner::Op
{
	Tracked<int8_t> active = {0, 0};
	Tracked<idx_t> succ = {IDX_MAX, IDX_MAX};
	const Preprocess::Op* prepr = nullptr;
};


struct Route_planner::Level
{
	idx_t next = IDX_MAX;
	dur_t dur = 0;
	tim_t lb = 0;
	Tracked<tim_t> time = {0, 0};
};


struct Route_planner::Route
{
	Tracked<int8_t> active = {0, 0};
	int8_t required = 0;
	int8_t frozen = 0;
	GRBVar var;
	const Preprocess::Route* prepr = nullptr;

	GRBLinExpr to_expr() const { return (required ? 1 : GRBLinExpr(var)); }
	void freeze();
	void unfreeze();
};
