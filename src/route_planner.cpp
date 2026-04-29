#include "route_planner.hpp"

#include <format>

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
	this->update_route_ops();
	this->sync_extern();
}


void Route_planner::optimize_routes()
{
	
}


void Route_planner::update_route_ops()
{
	for (auto& route : this->routes) {
		if (!route.active.changed()) {
			continue;
		}

		if (route.active.old) {
			for (auto o : route.prepr->ops) {
				this->ops[o].active.curr = false;
			}
		}

		if (route.active.curr) {
			for (auto o : route.prepr->ops) {
				this->ops[o].active.curr = true;
			}
		}
	}

	for (auto& op : this->ops) {
		if (op.active) {
			for (auto s : op.prepr->inst->succ) {
				if (this->ops[s].active.curr) {
					op.succ.curr = s;
					break;
				}
			}
		}
		else {
			op.succ.curr = IDX_MAX;
		}
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


void Route_planner::sync_extern()
{
	this->op_change.clear();
	this->op_succ_change.clear();

	this->get_op_changes();
	this->link_graph.op_change(this->op_change, this->op_succ_change);
	this->link_graph.sync();
	this->chunk_mngr.op_change(this->op_change);
	this->chunk_mngr.sync_state();

	this->level_time_change.clear();
	this->get_time_changes();
	this->chunk_mngr.time_change(this->level_time_change);
}


void Route_planner::get_op_changes()
{
	for (auto o : this->inst.ops_range()) {
		auto& op = this->ops[o];
		if (op.active.changed()) {
			this->op_change.push_back({o, op.active.get_change()});
			op.active.snap();
		}

		if (op.succ.changed()) {
			if (op.succ.old < IDX_MAX) {
				this->op_succ_change -= {o, op.succ.old};
			}
			if (op.succ.curr < IDX_MAX) {
				this->op_succ_change += {o, op.succ.curr};
			}

			op.succ.snap();
		}
	}
}


void Route_planner::get_time_changes()
{
	for (auto l : this->prepr.levels_range()) {
		auto& level = this->levels[l];
		if (level.time.changed()) {
			this->level_time_change.push_back({l, level.time.curr});
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

		GRBLinExpr lhs = 0.0;
		GRBLinExpr rhs = 0.0;

		for (auto& pred : junct.pred) {
			auto& op = this->prepr.ops[pred.op];
			lhs += this->routes[op.route].to_expr();
		}

		for (auto& succ : junct.pred) {
			auto& op = this->prepr.ops[succ.op];
			rhs += this->routes[op.route].to_expr();
		}

		auto constr = this->model.addConstr(rhs == lhs,
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
	this->init_chunks();

	this->init_model();
}


void Route_planner::init_model()
{
	this->model.set(GRB_IntAttr_ModelSense, GRB_MINIMIZE);
	this->find_req_routes();
	this->add_route_vars();
	this->add_flow_constr();
}


void Route_planner::init_ops()
{
	this->ops.resize(this->inst.n_ops());
	for (auto o : this->inst.ops) {
		this->ops[o].prepr = &this->prepr.ops[o];
	}
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


void Route_planner::init_chunks()
{
	this->chunk_price.resize(this->prepr.n_chunks());
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


