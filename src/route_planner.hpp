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

	double price_mult = 0.001;

	const Instance& inst;
	const Preprocess& prepr;

	Flag op_active;
	Flag op_change;

	Route_planner(const Preprocess& prepr, Link_graph& link_graph, 
		Chunk_manager& chunk_mrng, GRBEnv& grb_env);
	~Route_planner();

	std::vector<std::vector<idx_pr>> train_conflicts;
	std::vector<idx_t> conf_chain_len = {};

	void make_init_routes();
	void optimize_routes();

	void estimate_level_times();
	void make_train_conflicts();

	void verify_ops();

	size_t get_cost_sum();

private:
	struct Flow_cons;
	
	Link_graph& link_graph;
	Chunk_manager& chunk_mngr;
	GRBModel model;
    
	std::vector<idx_t> flag_list = {};

	std::vector<Route> routes = {};

	std::vector<tim_t> level_time = {};

	std::vector<GRBConstr> flow_constr = {};
	std::vector<GRBConstr> inva_constr = {};
	
	void get_random_routes();

	template<typename C>
	void price_routes(C& routes);
	size_t get_train_cost(idx_t train);

	template<typename C>
	void update_values(C& routes);

	template<typename C>
	void update_ops(C& routes);

	void update_all_ops();

	void find_req_routes();
	void add_route_vars();
	void add_flow_constr();
	void add_inva_constr();

	bool optimize_model();

	void freeze_all();
	void unfreeze_all();

	template<typename C>
	void freeze_routes(C& routes);

	template<typename C>
	void unfreeze_routes(C& routes);

	void init_data();
	void init_ops();
	void init_levels();
	void init_routes();
	void init_model();
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
