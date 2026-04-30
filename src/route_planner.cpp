#include "route_planner.hpp"

#include <format>
#include <random>
#include <algorithm>

#include "utils/stl_print.hpp"

using namespace std;


Route_planner::Route_planner(const Preprocess& prepr, Link_graph& link_graph, 
	Chunk_manager& chunk_mngr, GRBEnv& grb_env)
	: inst(prepr.inst), prepr(prepr), link_graph(link_graph), 
	  chunk_mngr(chunk_mngr), model(GRBModel(grb_env))
{
	this->init_data();
}


Route_planner::~Route_planner()
{
	
}


void Route_planner::estimate_level_times()
{
	size_t n_levels = this->prepr.n_levels();
	auto levels_range = this->prepr.levels_range();

	size_t n_iter = 1000;

	double cumu_count[n_levels];
	double cumu_time[n_levels];
	for (auto l : levels_range) {
		cumu_count[l] = 0;
		cumu_time[l] = 0;
	}


	double curr_time[n_levels];

	for (size_t i = 0; i < n_iter; i++) {
		for (auto l : levels_range) {
			curr_time[l] = -1.0;
		}


		for (auto& train : this->inst.trains) {
			auto o = train.op_first;

			while (true) {
				auto& op = this->prepr.ops[o];
				this->routes[op.route].active.curr = true;
				
				curr_time[op.level.start] = MAX(curr_time[op.level.start], op.inst->start_lb);
				curr_time[op.level.end] = MAX(curr_time[op.level.end], curr_time[op.level.start] + op.inst->dur);

				if (op.inst->succ.size() == 0) { break ; }
				auto s = op.inst->succ.get_random_item();

				o = s;
			}
		}


		for (auto l : levels_range) {
			if (curr_time[l] >= 0) {
				cumu_count[l]++;
				cumu_time[l] += curr_time[l];
			}
		}
	}

	for (auto l : levels_range) {
		assert(cumu_count[l] >= 1);
		this->level_time[l] = round(cumu_time[l]/cumu_count[l]);
	}

	// cout << this->level_time << endl;

	vector<pair<idx_t, tim_t>> changes;
	changes.reserve(n_levels);

	for (auto l : levels_range) {
		changes.push_back({l, this->level_time[l]});
	}

	Flag level_change(this->prepr.n_levels());
	this->chunk_mngr.time_change(level_change);
}


void Route_planner::make_train_conflicts()
{
	this->op_change.fill(1);
	this->op_active.fill(1);

	this->chunk_mngr.op_change(this->op_change);
	this->chunk_mngr.sync_state(this->op_active);
	this->chunk_mngr.sync_time(this->level_time);

	std::vector<idx_pr> confs;
	this->chunk_mngr.get_all_conflicts(confs, 0.3);

	this->train_conflicts.resize(this->inst.n_trains());
	
	for (auto x : confs) {
		if (x.first > x.second) {
			swap(x.first, x.second);
		}

		idx_t t1 = this->chunk_mngr.train_idx[x.first];
		idx_t t2 = this->chunk_mngr.train_idx[x.second];
		
		this->train_conflicts[t1].push_back(x);
		this->train_conflicts[t2].push_back(x);
	}

	size_t max_conf = 0;
	for (auto t : this->inst.trains_range()) {
		max_conf = MAX(max_conf, this->train_conflicts[t].size());
	}

	this->conf_chain_len.reserve(max_conf);
}


void Route_planner::make_init_routes()
{
	this->get_random_routes();
	this->freeze_all();

	this->update_all_ops();
	this->op_change.fill(1);
	this->link_graph.op_change(this->op_change);
	this->link_graph.sync_links(this->op_active);
	this->op_change.clear();
}


void Route_planner::optimize_routes()
{
	vector<idx_t> sect_order;
	sect_order.reserve(this->prepr.n_sects());

	for (auto s : this->prepr.sects_range()) {
		if (this->prepr.sects[s].n_routes() == 1) {
			continue;
		}

		sect_order.push_back(s);
	}


	shuffle(sect_order.begin(), sect_order.end(), default_random_engine());
	for (auto s : sect_order) {

		auto& sect = this->prepr.sects[s];

		this->price_routes(sect.routes);
		this->unfreeze_routes(sect.routes);

		this->optimize_model();

		this->update_values(sect.routes);
		this->update_ops(sect.routes);
		this->link_graph.op_change(this->op_change);
		this->link_graph.sync_links(this->op_active);
		this->op_change.clear();

		this->freeze_routes(sect.routes);

		// cout << "curr cost: " << this->get_cost_sum() << endl;
	}

	this->link_graph.op_change(this->op_change);
	this->link_graph.sync_links(this->op_active);
	this->op_change.clear();
}



template<typename C>
void Route_planner::price_routes(C& routes)
{
	assert(routes.size() > 0);
	idx_t train = this->prepr.routes[routes[0]].train;

	for (idx_t r : routes) {
		auto& route = this->routes[r];
		route.active.curr = true;
		assert(route.prepr->train == train);
	}

	
	this->update_ops(routes);
	this->link_graph.op_change(this->op_change);
	this->link_graph.sync_links(this->op_active);
	this->op_change.clear();

	size_t baseline_cost = this->get_train_cost(train);

	for (idx_t r : routes) {
		auto& route = this->routes[r];
		route.active.curr = false;

		this->update_ops(routes);
		this->link_graph.op_change(this->op_change);
		this->link_graph.sync_links(this->op_active);
		this->op_change.clear();


		size_t curr_cost = this->get_train_cost(train);
		assert(curr_cost <= baseline_cost);

		route.var.set(GRB_DoubleAttr_Obj, baseline_cost - curr_cost);

		route.active.curr = true;
	}
}


size_t Route_planner::get_train_cost(idx_t train)
{
	this->link_graph.get_chain_len(this->train_conflicts[train], this->conf_chain_len);

	size_t cost = 0;
	for (auto x : this->conf_chain_len) {
			cost += x;
	}

	return cost;
}


size_t Route_planner::get_cost_sum()
{
	size_t cost = 0;
	for (auto t : this->inst.trains_range()) {
		cost += this->get_train_cost(t);
	}

	return cost;
}



template<typename C>
void Route_planner::update_values(C& routes)
{
	this->op_change.clear();
	for (auto r : routes) {
		auto& route = this->routes[r];
		route.active = route.var.get(GRB_DoubleAttr_X) > 0.5;
	}
}


template<typename C>
void Route_planner::update_ops(C& routes)
{
	// this->op_change.clear();
	for (auto r : routes) {
		auto& route = this->routes[r];
		if (!route.active.changed()) {
			continue;
		}

		if (route.active.old) {
			for (auto o : route.prepr->ops) {
				this->op_active -= o;
				this->op_change += o;
			}
		}

		if (route.active.curr) {
			for (auto o : route.prepr->ops) {
				this->op_active += o;
				this->op_change += o;
			}
		}

		route.active.snap();
	}
}


void Route_planner::update_all_ops()
{
	auto rnge = this->prepr.routes_range();
	this->update_ops(rnge);
}



void Route_planner::get_random_routes()
{
	for (auto& route : this->routes) {
		route.active.curr = false;
	}

	for (auto& train : this->inst.trains) {
		auto o = train.op_first;

		while (true) {
			auto& op = this->prepr.ops[o];
			this->routes[op.route].active.curr = true;


			if (op.inst->succ.size() == 0) { break ; }
			auto s = op.inst->succ.get_random_item();

			o = s;
		}
	}
}



bool Route_planner::optimize_model()
{
	bool ret = false;
	int status = GBR_EXCEPTION;
	try {
		this->model.update();
		this->model.optimize();
		status =  this->model.get(GRB_IntAttr_Status);
	}
	catch (GRBException ex) {
		cout << "Route planner: gurobi exception " << ex.getMessage() << 
			", code = " << ex.getErrorCode() << endl;
	}
	
	switch (status) {
	  case GRB_OPTIMAL:
		ret = true;
		break;

	  case GRB_INFEASIBLE:
		break;
	
	  case GBR_EXCEPTION:
		break;

	  default:
		cout << "Route planner: unexpected optimization status = " << status << endl;
		break;
	}

	return ret;
}


void Route_planner::find_req_routes()
{
	for (auto& route : this->routes) {
		route.required = 0;
	}

	for (auto& sect : this->prepr.sects) {
		if (sect.routes.size() == 1) {
			this->routes[sect.routes[0]].required = 1;
		}
	}
}


void Route_planner::add_route_vars()
{
	for (auto& route : this->routes) {
		if (route.required) {
			continue;
		}

		route.var = this->model.addVar(0, 1, 0, GRB_BINARY, 
			format("route_{}", route.prepr->idx));
	}
}


void Route_planner::add_flow_constr()
{
	for (auto& junct : this->prepr.juncts) {
		if (!junct.is_route) {
			continue;
		}

		GRBLinExpr lhs = (double)(junct.n_pred() == 0);
		GRBLinExpr rhs = (double)(junct.n_succ() == 0);


		for (auto& pred : junct.pred) {
			auto& op = this->prepr.ops[pred.op];
			lhs += this->routes[op.route].to_expr();
		}

		for (auto& succ : junct.succ) {
			auto& op = this->prepr.ops[succ.op];
			rhs += this->routes[op.route].to_expr();
		}

		if (lhs.size() == 0 && rhs.size() == 0) {
			continue;
		}

		auto constr = this->model.addConstr(lhs == rhs,
			format("flow_{}", junct.idx));

		this->flow_constr.push_back(constr);
	}
}

template<typename C>
void Route_planner::freeze_routes(C& routes)
{
	for (auto r : routes) {
		auto& route = this->routes[r];
		route.freeze();
	}
}


template<typename C>
void Route_planner::unfreeze_routes(C& routes)
{
	for (auto r : routes) {
		auto& route = this->routes[r];
		route.unfreeze();
	}
}


void Route_planner::freeze_all()
{
	for (auto& route : this->routes) {
		route.freeze();
	}
}

void Route_planner::unfreeze_all()
{
	for (auto& route : this->routes) {
		route.unfreeze();
	}
}


void Route_planner::init_data()
{
	this->init_ops();
	this->init_levels();
	this->init_routes();
	this->init_model();
}


void Route_planner::init_model()
{
	this->model.set(GRB_IntAttr_ModelSense, GRB_MINIMIZE);
	
	this->find_req_routes();
	this->add_route_vars();
	this->add_flow_constr();
	this->model.write("route.lp");
}


void Route_planner::init_ops()
{
	this->op_active.set_n_items(this->inst.n_ops());
	this->op_change.set_n_items(this->inst.n_ops());
}

void Route_planner::init_levels()
{
	this->level_time.resize(this->prepr.n_levels());
}


void Route_planner::init_routes()
{
	size_t n_routes = this->prepr.n_routes();

	this->routes.resize(n_routes);
	
	for (auto r : this->prepr.routes_range()) {
		auto& route = this->routes[r];
		// route.idx = r;
		route.prepr = &this->prepr.routes[r];
	}
}



void Route_planner::Route::freeze()
{
	if (this->required || this->frozen) {
		return;
	}

	if (this->active.curr) {
		this->var.set(GRB_DoubleAttr_LB, 1.0);
	}
	else {
		this->var.set(GRB_DoubleAttr_UB, 0.0);
	}
	
	this->frozen = true;
}


void Route_planner::Route::unfreeze()
{
	if (this->required || !this->frozen) {
		return;
	}

	this->var.set(GRB_DoubleAttr_LB, 0.0);
	this->var.set(GRB_DoubleAttr_UB, 1.0);

	this->frozen = false;
}


