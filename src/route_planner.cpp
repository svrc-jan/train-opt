#include "route_planner.hpp"

#include <format>

using namespace std;


Route_planner::Route_planner(Solver& solver)
	: inst(solver.inst), prepr(solver.prepr), slvr(solver), model(GRBModel(solver.grb_env))
{

}


Route_planner::~Route_planner()
{
	
}


void Route_planner::get_random_ops(double dur_stretch)
{
	this->op_active.curr.clear();
	for (auto& x : this->op_succ) {
		x = IDX_MAX;
	}

	for (auto& level : this->levels) {
		level.next = IDX_MAX;
	}


	for (auto& train : this->inst.trains) {
		auto o = train.op_first;

		while (true) {
			auto& op = this->prepr.ops[o];
			this->op_active.curr += o;
			
			auto& level = this->levels[op.level.start];
			level.next = op.level.end;
			level.dur = op.inst->dur;
			level.lb = op.inst->start_lb;
			if (dur_stretch > 0.0) {
				level.stretch_dur(dur_stretch);
			}

			if (op.inst->succ.size() == 0) { break ; }
			auto s = op.inst->succ.get_random_item();

			this->op_succ[o] = s;
			o = s;
		}
	}

	this->op_dirty = this->op_active.old;
	this->op_dirty ^= this->op_active.curr;
}


void Route_planner::snap_ops()
{
	this->op_active.snap();
	this->op_dirty.clear();

	for (auto& x : this->op_succ) {
		x.snap();	
	}
}

void Route_planner::sync_graph()
{
	for (auto& level : this->levels) {
		if (level.lb.changed()) {
			this->slvr.event_graph.set_time_lb(level.idx, level.lb.curr);
			level.lb.snap();
		}
		
		if (level.next.changed() || level.dur.changed()) {
			this->slvr.event_graph.update_edge(level.edge_old(), level.edge_curr());
			level.next.snap();
			level.dur.snap();
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
}


void Route_planner::init_ops()
{
	size_t n_ops = this->inst.n_ops();

	this->op_active.curr.set_n_items(n_ops);
	this->op_active.snap();
	this->op_dirty.set_n_items(n_ops);

	this->op_succ.resize(n_ops, {IDX_MAX, IDX_MAX});
}

void Route_planner::init_levels()
{
	this->levels.resize(this->prepr.n_levels());
	for (auto l : this->prepr.levels_range()) {
		this->levels[l].idx = l;
	}
}


void Route_planner::init_routes()
{
	size_t n_routes = this->prepr.n_routes();

	this->routes.resize(n_routes);
	
	for (auto& route : this->prepr.routes) {
		this->routes[route.idx].prepr = &route;
	}
}


void Route_planner::find_req_routes()
{
	for (auto& route : this->routes) {
		route.is_req = 0;
	}

	for (auto& sect : this->prepr.sects) {
		if (sect.routes.size() == 1) {
			this->routes[sect.routes[0]].is_req = 1;
		}
	}
}


void Route_planner::add_route_vars()
{
	for (auto& route : this->routes) {
		if (route.is_req) {
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


void Route_planner::freeze_train(idx_t t)
{
	for (auto& route : this->prepr.trains[t].routes) {
		this->routes[route.idx].freeze();
	}
}

void Route_planner::unfreeze_train(idx_t t)
{
	for (auto& route : this->prepr.trains[t].routes) {
		this->routes[route.idx].unfreeze();
	}
}


void Route_planner::Route::freeze()
{
	if (this->is_req) {
		return;
	}

	this->var.set(GRB_DoubleAttr_LB, (double)this->value);
	this->var.set(GRB_DoubleAttr_UB, (double)this->value);
	this->is_frozen = true;
}


void Route_planner::Route::unfreeze()
{
	if (this->is_req) {
		return;
	}

	this->var.set(GRB_DoubleAttr_LB, 0.0);
	this->var.set(GRB_DoubleAttr_UB, 1.0);
	this->is_frozen = true;
}


