#include "route_planner.hpp"


Route_planner::Route_planner(Solver& solver)
	: inst(solver.inst), prepr(solver.prepr), slvr(solver), model(GRBModel(solver.grb_env))
{

}


Route_planner::~Route_planner()
{

}


void Route_planner::init_data()
{
	this->is_op_active.set_n_items(this->inst.n_ops());

	this->ops.resize(this->prepr.n_ops());
	for (auto& op : this->prepr.ops) {
		this->ops[op.idx].prepr = &op;
	}

	this->routes.resize(this->prepr.n_routes());
	for (auto& route : this->prepr.routes) {
		this->routes[route.idx].prepr = &route;
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
	this->route_set.clear();

	if (sect.routes.size() == 1) {
		this->route_set.insert(sect.routes[0]);
	}
	else {
		auto& level_first = this->prepr.levels[sect.level.start];
		idx_t l_last = sect.level.end;
		idx_t o = level_first.succ.get_random_item().op;
		
		while (true) {
			auto& op = this->prepr.ops[o];
			this->route_set.insert(op.route);

			assert(op.level.end <= l_last);
			if (op.level.end == l_last) {
				break;
			}

			o = op.inst->succ.get_random_item();
		}
	}


	for (auto r : this->route_set) {
		auto& route = this->routes[r];
		if (route.value == 0) {
			this->routes[r].value = 1;
			this->route_op_dirty += r;
			this->need_route_op_sync = true;
		}
	}
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

		for (auto o : route.prepr->ops) {
			auto& op = this->ops[o];
			op.active = route.active;

			this->op_graph_dirty += o;
			this->need_route_op_sync = true;

			for (auto c : op.prepr->chunks) {
				this->slvr.chunk_state_dirty += c;
				this->slvr.need_chunk_state_sync = true;
			}
		}
		
		route.active = route.value;
	}

	this->route_op_dirty.clear();
	this->need_route_op_sync = false;
}



void Route_planner::sync_op_graph()
{
	if (!this->need_op_graph_sync) {
		return;
	}

	this->sync_route_ops();

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

