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


void Route_planner::make_init_routes()
{
	this->assign_all_random_sections();
	this->assign_all_sections_dur(1.5);
	this->slvr.chunk_mngr->sync_res();

	this->freeze_all();
	bool feasible = this->optimize_model();
	assert(feasible);
}


void Route_planner::plan_section_range(const Interval<idx_t>& section_ivl)
{
	auto& sect_start = this->prepr.sects[section_ivl.start];
	auto& sect_end = this->prepr.sects[section_ivl.end];

	assert(sect_start.train == sect_end.train);
	assert(sect_start.idx <= sect_end.idx);

	Interval<idx_t> level_ivl = {sect_start.level.start, sect_end.level.end};
	// Interval<tim_t> time_ivl = {this->slvr.time(level_ivl.start), this->slvr.time(level_ivl.end)};
}


void Route_planner::init_data()
{
	this->init_ops();
	this->init_routes();
	this->init_model();
}


void Route_planner::init_ops()
{
	size_t n_ops = this->inst.n_ops();

	this->is_op_active.set_n_items(n_ops);
	this->op_graph_dirty.set_n_items(n_ops);
	this->ops.resize(n_ops);

	for (auto& op : this->prepr.ops) {
		this->ops[op.idx].prepr = &op;
	}
}


void Route_planner::init_routes()
{
	size_t n_routes = this->prepr.n_routes();

	this->is_route_active.set_n_items(n_routes);
	this->route_op_dirty.set_n_items(n_routes);
	this->routes.resize(n_routes);
	
	for (auto& route : this->prepr.routes) {
		this->routes[route.idx].prepr = &route;
	}
}


void Route_planner::init_model()
{
	this->model.set(GRB_IntAttr_ModelSense, GRB_MINIMIZE);
	this->find_req_routes();
	this->add_route_vars();
	this->add_flow_constr();
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


void Route_planner::freeze_section(const Preprocess::Section& sect)
{
	for (auto r : sect.routes) {
		this->routes[r].freeze();
	}
}

void Route_planner::unfreeze_section(const Preprocess::Section& sect)
{
	for (auto r : sect.routes) {
		this->routes[r].unfreeze();
	}
}


void Route_planner::assign_all_random_sections()
{
	for (auto& sect : this->prepr.sects) {
		this->assign_random_section(sect);
	}
}


void Route_planner::assign_random_section(const Preprocess::Section& sect)
{
	auto& route_set = this->assign_random_route_set;
	route_set.clear();

	if (sect.is_single_route) {
		route_set.insert(sect.routes[0]);
	}

	else {
		auto& level_first = this->prepr.levels[sect.level.start];
		idx_t l_last = sect.level.end;
		idx_t o = level_first.succ.get_random_item().op;
		
		while (true) {
			auto& op = this->prepr.ops[o];
			route_set.insert(op.route);

			assert(op.level.end <= l_last);
			if (op.level.end == l_last) {
				break;
			}

			o = op.inst->succ.get_random_item();
		}
	}


	for (auto r : this->assign_random_route_set) {
		auto& route = this->routes[r];
		if (route.value == 0) {
			this->routes[r].value = 1;
			this->route_op_dirty += r;
			this->need_route_op_sync = true;
		}
	}
}

void Route_planner::assign_all_sections_dur(double stretch)
{
	for (auto& sect : this->prepr.sects) {
		this->assign_section_dur(sect, stretch);
	}
}


void Route_planner::assign_section_dur(const Preprocess::Section& sect, double stretch)
{
	assert(stretch >= 0.0);

	for (auto r : sect.routes) {
		auto& route = this->routes[r];
		for (auto o : route.prepr->ops) {
			auto& op = this->ops[o];
			size_t new_dur = MIN(round(op.prepr->inst->dur*(1.0 + stretch)), DUR_MAX - 1);
			assert(new_dur < DUR_MAX);
			this->assign_op_dur(op, new_dur);
		}
	}
}



void Route_planner::assign_op_dur(Op& op, dur_t new_dur)
{
	if (new_dur != op.dur && op.active) {
		this->op_graph_dirty += op.prepr->idx;
		this->need_op_graph_sync = true;
	}

	op.dur = new_dur;
}


void Route_planner::sync_route_ops()
{
	if (!this->need_route_op_sync) {
		return;
	}

	this->route_op_dirty.get_true_list(this->slvr.need_list);
	for (auto r : this->slvr.need_list) {
		auto& route = this->routes[r];

		if (route.active == route.value) {
			continue;
		}

		route.active = route.value;
		if (route.value) {
			this->is_route_active += r;
		}
		else {
			this->is_route_active -= r;
		}

		for (auto o : route.prepr->ops) {
			auto& op = this->ops[o];
			op.active = route.active;

			if (route.value) {
				this->is_op_active += o;
			}
			else {
				this->is_op_active -= o;
			}

			this->op_graph_dirty += o;
			this->need_op_graph_sync = true;

			for (auto c : op.prepr->chunks) {
				this->slvr.chunk_mngr->state_change(c);
			}
		}
	}

	this->route_op_dirty.clear();
	this->need_route_op_sync = false;
}



void Route_planner::sync_op_graph()
{
	this->sync_route_ops();

	if (!this->need_op_graph_sync) {
		return;
	}
	

	this->op_graph_dirty.get_true_list(this->slvr.need_list);
	for (auto o : this->slvr.need_list) {
		auto& op = this->ops[o];

		Edge new_edge = op.to_edge();
		if (op.curr_edge != new_edge) {
			this->slvr.event_graph.update_edge(op.curr_edge, new_edge);
			this->slvr.graph_time_change();

			op.curr_edge = new_edge;
		}
	
		
		if (op.active) {
			bool lb_diff = this->slvr.event_graph.set_time_lb(
				op.prepr->level.start, op.prepr->inst->start_lb);
			
			if (lb_diff) {
				this->slvr.graph_time_change();
			}
		}
	}

	this->op_graph_dirty.clear();
	this->need_op_graph_sync = false;
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


