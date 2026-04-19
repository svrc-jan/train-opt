#include "solver.hpp"

#include <cmath>
#include "utils/stl_print.hpp"

using namespace std;


Solver::Solver(const Preprocess& prepr, GRBEnv& grb_env)
	: inst(prepr.inst), prepr(prepr), grb_env(grb_env), model(grb_env)
{
	this->init_data();
	this->event_graph.set_n_vtx(prepr.n_levels());
}


Solver::~Solver()
{

}


void Solver::init_data()
{
	this->init_objs();
	this->init_routes();
}


void Solver::init_objs()
{
	this->objs.resize(this->prepr.n_objs());
	for (auto& obj_p : this->prepr.objs) {
		auto& obj = this->objs[obj_p.idx];
		obj.prepr = &obj_p;
	}
}


void Solver::init_routes()
{
	this->n_routes = this->prepr.n_routes();
	this->routes.resize(this->n_routes);

	for (auto& route_p : this->prepr.routes) {
		auto& route = this->routes[route_p.idx];
		route.prepr = &route_p;
	}
}


void Solver::solve()
{
	for (auto t : this->inst.trains_range()) {
		this->solver_loop();
	}
}


void Solver::solver_loop()
{
	while (this->state != SLVR_DONE && this->state != SLVR_FAIL) {
		switch (this->state)	{
		  case SLVR_OPTIMIZE_MODEL:
			this->optimize_model();
			break;
		
		  case SLVR_UPDATE_VALUES:
		    this->update_values();
			break;
		
		  case SLRV_UPDATE_GRAPH:
			this->update_graph();
			break;

		  case SLVR_UPDATE_OBJ:
			this->update_objs();
			break;

		  case SLVR_ADD_CONFLICT:
			this->add_conflict();
			break;
		
		  default:
			assert(this->state == SLVR_DONE || this->state == SLVR_FAIL);
			break;
		}
	}

	if (this->state == SLVR_FAIL) {
		cout << "solver failed" << endl;
	}
}




void Solver::optimize_model()
{
	this->state = SLVR_UPDATE_VALUES; // if not fail
	int status;
	try {
		this->model.update();
		this->model.optimize();
		status = this->model.get(GRB_IntAttr_Status);
		if (status == GRB_OPTIMAL) {
			this->obj_val = this->model.get(GRB_DoubleAttr_ObjVal);
		}
		
	}
	catch (const GRBException& ex) {
		cout << "ERROR: optimization exception: " << ex.getMessage() << ", code: " << ex.getErrorCode() << endl;
		this->state = SLVR_FAIL;
	}

	if (status != GRB_OPTIMAL) {
		cout << "ERROR: model optim failed, status: " << status << endl;
		this->state = SLVR_FAIL;
	}
}


void Solver::update_values()
{
	bool need_graph_update = false;

	for (auto& obj : this->objs) {
		obj.value = round(obj.var.get(GRB_DoubleAttr_X));
	}

	for (auto& route : this->routes) {
		if (route.in_model) {
			route.value = route.var.get(GRB_DoubleAttr_X) > 0.5;
		}
	}

	for (auto& conf : this->conflicts) {
		if (conf.in_model) {

			uint8_t new_val = conf.var.get(GRB_DoubleAttr_X) > 0.5;
			if (new_val != conf.value) {
				conf.graph_update = true;
			}
		}
	}

}


void Solver::update_graph()
{
	bool changed = false;

	if (this->update_route_edges()) {
		changed = true;
	}

	if (this->update_conf_edges()) {
		changed = true;
	}

	this->state = SLVR_UPDATE_OBJ;
	if (changed) {
		if (this->event_graph.update()) {
			this->add_cycle_cons();
			this->state = SLVR_OPTIMIZE_MODEL;
		}
	}
}


bool Solver::update_route_edges()
{
	bool changed = false;
	for (auto& route : this->routes) {
		if (route.in_graph == 0 && route.value == 1) {
			for (auto o : route.prepr->ops) {
				auto& op = this->prepr.ops[o];

				Edge edge = {{op.level.start, op.level.end},
					route.prepr->idx, op.inst->dur};
			
				this->event_graph.add_edge(edge);
			}
			route.in_graph = 1;
			changed = true;
		}
		
		if (!route.in_graph == 1 && route.value == 0) {
			for (auto o : route.prepr->ops) {
				auto& op = this->prepr.ops[o];

				Edge edge = {{op.level.start, op.level.end},
					route.prepr->idx, op.inst->dur};

				bool removed = this->event_graph.remove_edge(edge);
				assert(removed);
			}
			route.in_graph = 1;
			changed = true;
		}
	}

	return changed;
}


bool Solver::update_conf_edges()
{
	assert(this->n_routes + this->conflicts.size() < EDG_MAX);

	bool changed = false;

	for (auto& conf : this->conflicts) {
		if (conf.graph_update == 0) {
			continue;
		}
		

		edg_t edge_idx = (edg_t)conf.idx + (edg_t)this->n_routes;
		Edge new_edge;
		if (conf.value == 1) {
			new_edge = {{conf.chunks.first->level.end, conf.chunks.second->level.start},
				conf.chunks.first->time, edge_idx};
		}
		else {
			new_edge = {{conf.chunks.second->level.end, conf.chunks.first->level.end},
				conf.chunks.second->time, edge_idx};
		}

		if (new_edge != conf.graph_edge) {
			if (conf.graph_edge.v.start != EDG_MAX) {
				bool removed = this->event_graph.remove_edge(conf.graph_edge);
				assert(removed);
			}

			this->event_graph.add_edge(new_edge);
			conf.graph_edge = new_edge;

			bool changed = false;
		}
	}

	return changed;
}


vector<Solver::Var_assign> Solver::collect_assigns(const vector<Event_graph::Vertex_edge>& path)
{
	this->assign_set.clear();
	for (auto& x : path) {
		if (x.e < EDG_MAX) {
			assert(x.e < this->n_routes + this->conflicts.size());
			this->assign_set.insert(x.e);
		}
	}

	vector<Solver::Var_assign> assigns = {};
	for (auto& x : this->assign_set) {
		if (x < this->n_routes) {
			auto& route = this->routes[x];
			if (route.in_graph) {
				assigns.push_back({route.var, route.value});
			}
		}
		else {
			auto& conf = this->conflicts[x - this->n_routes];
		}
	}	

	return assigns;
}


void Solver::add_cycle_cons()
{
	auto& cycle = this->event_graph.get_shortest_cycle();
	
	Cycle_cons cons;
	cons.assigns = this->collect_assigns(cycle);
	cout << cons.assigns << endl;
	
	assert(cons.assigns.size() >= 1);
	cons.add_to_model(this->model);

	this->cycle_cons.push_back(cons);
}


void Solver::update_objs()
{
	if (add_obj_cons()) {
		this->state = SLVR_OPTIMIZE_MODEL
	}
}

bool Solver::add_obj_cons()
{
	tim_t max_diff = 0;
	tim_t max_delay = 0;
	const Obj* max_obj = nullptr;
	

	for (auto& obj : this->objs) {
		if (obj.prepr->is_bin && obj.value == 1) {
			continue;
		}

		tim_t tim = this->event_graph.time[obj.prepr->level];
		if (tim < obj.prepr->threshold) {
			continue;
		}

		tim_t delay = tim - obj.prepr->threshold;
		if (delay <= obj.value) {
			continue;
		}

		time_t diff = obj.prepr->is_bin ? 
			obj.prepr->coeff : 
			obj.prepr->coeff*(delay - obj.value);

		if (max_diff < diff) {
			max_diff = diff;
			max_delay = delay;
			max_obj = &obj;
		}
	}

	if (max_obj == nullptr) {
		return false;
	}

	auto& path = this->event_graph.get_critical_path(max_obj->prepr->level);

	Path_cons cons = {
		.in_model = false,
		.delay = max_delay,
		.obj = max_obj,
		.assigns = this->collect_assigns(path)
	};
	
	cons.add_to_model(this->model);
	this->path_cons.push_back(cons);

	return true;
}


bool Solver::add_conflict()
{	

	
	for (auto& x : this->res_ints) {
		x.clear();
	}

	auto res_range = this->inst.res_range();
	for (auto t : this->inst.trains_range()) {
		for (auto r : res_range) {
			auto& ru = this->res_use[t][r];

			if (ru.level.start < IDX_MAX) {			
				auto& ri = this->res_ints[r];
				tim_t start = this->event_graph.time(ru.level.start);
				tim_t end = this->event_graph.time(ru.level.end);
				ri.push_back({t, r, {start, end}});
			}
		}
	}

	tim_t earliest = TIM_MAX;
	Conflict conf;

	for (auto ri : this->res_ints) {
		sort(ri.begin(), ri.end());

		size_t ri_size = ri.size();
		for (size_t i = 0; i + 1 < ri_size; i++) {
			auto& a = ri[i];
			auto& b = ri[i+1];

			if (b.tim.start >= earliest) {
				break;
			}

			if (a.tim.end > b.tim.end) {
				conf.res = a.res;
				if (a.train < b.train) {
					conf.train = {a.train, b.train};
				}
				else {
					conf.train = {b.train, a.train};
				}

				earliest = b.tim.end;
				break;
			}
		}
	}

	if (earliest == TIM_MAX) {
		return false;
	}

	
	conf.var = this->conf_vars.size();
	this->conflicts.push_back(conf);

	cout << "add conflict " << conf.res << " : " << conf.train << ", v" << conf.var << endl; 

	
	auto var = this->model.addVar(0, 1, 0, GRB_BINARY);
	this->conf_vars.push_back(var);
	this->conf_values.push_back(0);


	return true;
}

void Solver::freeze_conflicts()
{
	for (auto& x : this->conflicts) {
		if (x.var < IDX_MAX) {
			x.freeze = this->conf_values[x.var];
			x.var = IDX_MAX;
		}
	}
}


void Solver::Cycle_cons::add_to_model(GRBModel& model)
{
	if (this->in_model) {
		return;
	}
	
	GRBLinExpr expr(0);
	for (auto x : this->assigns) {
		expr += x.to_expr();
	}

	this->model_cons = model.addConstr(expr <= assigns.size() - 1);
	this->in_model = true;
}


void Solver::Cycle_cons::remove_from_model(GRBModel& model)
{
	if (this->in_model) {
		model.remove(this->model_cons);
		this->in_model = false;
	}
}


void Solver::Path_cons::add_to_model(GRBModel& model)
{
	if (this->in_model) {
		return;
	}
	
	GRBLinExpr expr(0);
	for (auto x : this->assigns) {
		expr += x.to_expr();
	}

	size_t n_assigns = this->assigns.size();

	if (this->obj->prepr->is_bin) {
		this->model_cons = model.addConstr((expr - n_assigns + 1) <= this->obj->var);
	}
	else {
		this->model_cons = model.addConstr((expr - n_assigns + 1)*this->delay <= this->obj->var);
	}
	
	this->in_model = true;
}


void Solver::Path_cons::remove_from_model(GRBModel& model)
{
	if (this->in_model) {
		model.remove(this->model_cons);
		this->in_model = false;
	}
}

