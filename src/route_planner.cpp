#include "route_planner.hpp"

#include <format>
#include <random>
#include <algorithm>

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


void Route_planner::make_init_routes()
{
	this->get_random_routes();

	this->op_change.clear();
	this->update_all_ops();
	this->link_graph.op_change(this->op_change);
	this->link_graph.sync_links(this->op_active);
	this->link_graph.update_max_chain();

	this->freeze_all();
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

	size_t n_sect = round(0.1*sect_order.size());

	vector<idx_t> routes;

	size_t n_iter = 20;

	for (size_t i = 0; i < n_iter; i++) {
		// cout << i + 1 << "/" << n_iter << endl;

		shuffle(sect_order.begin(), sect_order.end(), default_random_engine());

		routes.clear();

		for (size_t j = 0; j < n_sect; j++) {
			auto& sect = this->prepr.sects[sect_order[j]];
			for (auto r : sect.routes) {
				routes.push_back(r);
			}
		}

		sort(routes.begin(), routes.end());

		for (auto r : routes) {
			auto& route = this->routes[r];
			route.unfreeze();
		}

		this->update_price(routes);
		bool ret = this->optimize_model();
		assert(ret);

		this->update_values(routes);
		this->update_ops(routes);

		cout << i << "op_changes: " << this->op_change.get_true_count() << 
			"median chain" << link_graph.median_chain() << endl;

		this->link_graph.op_change(this->op_change);
		this->link_graph.sync_links(this->op_active);
		this->link_graph.update_max_chain();

		for (auto r : routes) {
			auto& route = this->routes[r];
			route.freeze();
		}
	}
}







void Route_planner::update_values(const vector<idx_t>& routes)
{
	for (auto r : routes) {
		auto& route = this->routes[r];
		route.active = route.var.get(GRB_DoubleAttr_X) > 0.5;
	}
}


void Route_planner::update_all_ops()
{
	vector<idx_t> rnge(this->prepr.n_routes());

	for (auto r : this->prepr.routes_range()) {
		rnge[r] = r;
	}

	this->update_ops(rnge);
}


void Route_planner::update_ops(const vector<idx_t>& routes)
{
	this->op_change.clear();
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



void Route_planner::update_price(const vector<idx_t>& routes)
{
	for (auto r : routes) {
		auto& route = this->routes[r];

		uint8_t max_chain = 0;
		for (auto o : route.prepr->ops) {
			auto& op = this->prepr.ops[o];

			for (auto c : op.chunks) {
				max_chain = MAX(max_chain, this->link_graph.chunk_max_chain[c]);
			}
		}

		route.price = this->price_mult*((double)max_chain) + 
			(1.0 - this->price_mult)*route.price;

		route.var.set(GRB_DoubleAttr_Obj, route.price);
	}
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


void Route_planner::update_level_time(idx_t l_start)
{
	idx_t l = l_start;
	while (true) {
		auto& level = this->levels[l];
		level.time.curr = MAX(level.time.curr, level.lb);

		if (level.next == IDX_MAX) {
			break;
		}

		auto& level_next = this->levels[level.next];
		level_next.time.curr = level.time.curr + (tim_t)level.dur;

		l = level.next;
	}
}



void Route_planner::get_time_changes()
{
	for (auto l : this->prepr.levels_range()) {
		auto& level = this->levels[l];
		if (level.time.changed()) {
			level.time.snap();
		};
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

	this->obj = this->model.addVar(0, 1000, 1, GRB_CONTINUOUS, "obj");
}


void Route_planner::init_ops()
{
	this->op_active.set_n_items(this->inst.n_ops());
	this->op_change.set_n_items(this->inst.n_ops());
}

void Route_planner::init_levels()
{
	this->levels.resize(this->prepr.n_levels());
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


