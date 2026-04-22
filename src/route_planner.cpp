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


void Route_planner::plan_section_range(const Interval<idx_t>& section_ivl)
{
	auto& sect_start = this->prepr.sects[section_ivl.start];
	auto& sect_end = this->prepr.sects[section_ivl.end];

	assert(sect_start.train == sect_end.train);
	assert(sect_start.idx <= sect_end.idx);

	Interval<idx_t> level_ivl = {sect_start.level.start, sect_end.level.end};
	// Interval<tim_t> time_ivl = {this->slvr.time(level_ivl.start), this->slvr.time(level_ivl.end)};

	this->make_levels(level_ivl);
	this->make_plan_routes(section_ivl);
	this->make_plan_chunks();

	this->optimize_model();
}


void Route_planner::make_levels(const Interval<idx_t>& level_ivl)
{
	this->slvr.sync_level_times();

	this->make_level_bounds(level_ivl);

	for (auto l : level_ivl.range_drop()) {
		auto& level = this->levels[l];
		assert(!level.is_fixed);
		level.in_model = true;

		assert(level.lb <= level.ub);

		level.var = this->model.addVar(level.lb, level.ub, 0, GRB_CONTINUOUS,
			format("level_{}", level.prepr->idx));

		this->plan_vars.push_back(level.var);
	}
}


void Route_planner::make_level_bounds(const Interval<idx_t>& level_ivl)
{
	this->slvr.sync_level_times();

	for (auto l : level_ivl.range_inc()) {
		auto& level = this->levels[l];
		level.in_model = false;
		if ((l == level_ivl.start) || (l == level_ivl.end)) {
			level.is_fixed = true;
			level.lb = this->slvr.time(l);
			level.ub = level.lb;
		}
		else {
			level.is_fixed = false;
			level.lb = TIM_MAX;
			level.ub = 0;
		}
	}

	this->propagate_level_lbs(level_ivl);
	this->propagate_level_ubs(level_ivl);
}


void Route_planner::propagate_level_lbs(const Interval<idx_t>& level_ivl)
{
	for (idx_t l = level_ivl.start; l < level_ivl.end; l++) {
		auto& level = this->levels[l];
		if (!level.prepr->is_req) {
			continue;
		}

		for (auto& succ : level.prepr->succ) {
			auto& level_succ = this->levels[succ.level];
			if (level_succ.is_fixed) {
				continue;
			}

			auto& op = this->inst.ops[succ.op];
			level_succ.lb = MIN(level_succ.lb, level.lb + op.dur);
		}
	}
}


void Route_planner::propagate_level_ubs(const Interval<idx_t>& level_ivl)
{
	for (idx_t l = level_ivl.end; l > level_ivl.start; l--) {
		auto& level = this->levels[l];
		if (!level.prepr->is_req) {
			continue;
		}

		for (auto& pred : level.prepr->pred) {
			auto& level_pred = this->levels[pred.level];
			if (level_pred.is_fixed) {
				continue;
			}

			auto& op = this->inst.ops[pred.op];
			level_pred.ub = MAX(level_pred.ub, level.ub - op.dur);
		}
	}
}


void Route_planner::make_plan_routes(const Interval<idx_t>& section_ivl)
{
	this->plan_chunk_set.clear();

	for (auto s : section_ivl.range_inc()) {
		auto& sect = this->prepr.sects[s];
		for (auto r : sect.routes) {
			auto& route = this->routes[r];
			route.unfreeze();
			for (auto o : route.prepr->ops) {
				auto& op = this->prepr.ops[o];

				for (auto c : op.chunks) {
					this->plan_chunk_set.insert(c);
				}

				GRBTempConstr cons = 
					this->levels[op.level.start].to_expr() + 
					route.to_expr()*op.inst->dur <=
					this->levels[op.level.end].to_expr();

				this->plan_constrs.push_back(this->model.addConstr(cons,
					format("dur_{}", op.idx)));
			}
		}
	}
}


void Route_planner::make_plan_chunks()
{
	for (auto c : this->plan_chunk_set) {
		auto& chunk = this->chunks[c];

		chunk.lb = {TIM_MAX, TIM_MAX};
		chunk.ub = {0, 0};

		
		for (auto o : chunk.prepr->ops) {
			auto& op = this->prepr.ops[o.idx];

			auto& level_start = this->levels[op.level.start];
			auto& level_end = this->levels[op.level.end];
			

			size_t rel_time = o.rel_time + round(
				op.inst->dur*this->res_dur_stretch);
			rel_time = MIN(rel_time, DUR_MAX - 1);

			chunk.lb.start = MIN(chunk.lb.start, level_start.lb);
			chunk.lb.end = MIN(chunk.lb.end, level_end.lb + rel_time);

			chunk.ub.start = MAX(chunk.ub.start, level_start.ub);
			chunk.ub.end = MAX(chunk.ub.end, level_end.ub + rel_time);

			assert(chunk.lb.start <= chunk.ub.start);
			assert(chunk.lb.end <= chunk.ub.end);

		}

		chunk.var.start = this->model.addVar(chunk.lb.start, chunk.ub.start, 0, GRB_CONTINUOUS,
			format("chunk_{}_start", chunk.prepr->idx));

		chunk.var.end = this->model.addVar(chunk.lb.end, chunk.ub.end, 0, GRB_CONTINUOUS,
			format("chunk_{}_start", chunk.prepr->idx));

		
		this->plan_vars.push_back(chunk.var.start);
		this->plan_vars.push_back(chunk.var.end);

		
		for (auto o : chunk.prepr->ops) {
			auto& op = this->prepr.ops[o.idx];

			auto& level_start = this->levels[op.level.start];
			auto& level_end = this->levels[op.level.end];
			

			size_t rel_time = o.rel_time + round(
				op.inst->dur*this->res_dur_stretch);

			rel_time = MIN(rel_time, DUR_MAX - 1);

			size_t M = chunk.ub.start - level_start.lb;
			GRBTempConstr cons_start = chunk.var.start <=
				level_start.to_expr() + M*(1 - this->routes[op.route].to_expr());

			M = level_end.ub + rel_time - chunk.lb.end;
			GRBTempConstr cons_end = chunk.var.end >=
				level_end.to_expr() + rel_time - M*(1 - this->routes[op.route].to_expr());

			this->plan_constrs.push_back(this->model.addConstr(cons_start,
				format("chunk_{}_start_{}", chunk.prepr->idx, o.idx)));

			this->plan_constrs.push_back(this->model.addConstr(cons_end,
				format("chunk_{}_end_{}", chunk.prepr->idx, o.idx)));
		}
	}

	this->slvr.sync_res_chunks();

	for (auto c : this->plan_chunk_set) {
		auto& chunk = this->chunks[c];
		idx_t train = chunk.prepr->train;

		for (auto other : this->slvr.res_chunks[chunk.prepr->res]) {
			bool before = other->time.end <= chunk.lb.start;
			bool after = other->time.start >= chunk.ub.end;

			if (before || after) {
				continue;
			}

			GRBVar var = this->model.addVar(0, GRB_INFINITY, 1, GRB_CONTINUOUS,
				format("overlap_{}_{}", c, other->prepr->idx));
		
			GRBTempConstr cons;

			if ((other->time.start + other->time.end) < (chunk.lb.start + chunk.ub.end)) {
				cons = (other->time.end - chunk.var.start <= var);
			}
			else {
				cons = (chunk.var.end - other->time.start <= var);
			}

			this->plan_vars.push_back(var);
			this->plan_constrs.push_back(this->model.addConstr(cons));
		}
	}
}


void Route_planner::init_data()
{
	size_t n_routes = this->prepr.n_routes();

	this->is_route_active.set_n_items(n_routes);
	this->route_op_dirty.set_n_items(n_routes);
	this->routes.resize(n_routes);
	
	for (auto& route : this->prepr.routes) {
		this->routes[route.idx].prepr = &route;
	}
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


void Route_planner::init_levels()
{
	size_t n_levels = this->prepr.n_levels();
	this->levels.resize(n_levels);

	for (auto& level : this->prepr.levels) {
		this->levels[level.idx].prepr = &level;
	}
}


void Route_planner::init_chunks()
{
	size_t n_chunks = this->prepr.n_chunks();
	this->chunks.resize(n_chunks);

	for (auto& chunk : this->prepr.chunks) {
		this->chunks[chunk.idx].prepr = &chunk;
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
				this->slvr.chunk_state_dirty += c;
				this->slvr.need_chunk_state_sync = true;
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
			this->slvr.need_level_time_sync = true;

			op.curr_edge = new_edge;
		}
	
		
		if (op.active) {
			bool lb_diff = this->slvr.event_graph.set_time_lb(
				op.prepr->level.start, op.prepr->inst->start_lb);
			
			if (lb_diff) {
				this->slvr.need_level_time_sync = true;
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


