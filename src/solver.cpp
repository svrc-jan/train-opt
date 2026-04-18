#include "solver.hpp"

#include <cmath>

using namespace std;


Solver::Solver(const Preprocess& prepr, GRBEnv& grb_env)
	: inst(prepr.inst), prepr(prepr), grb_env(grb_env), model(grb_env)
{
	size_t n_levels = this->prepr.n_levels();
	size_t n_trains = this->inst.n_trains();

	this->event_graph.set_n_vtx(n_levels);

	this->objs.resize(n_trains);

	this->level_dur.resize(n_levels);
	this->level_res.resize(n_levels);
	
	this->init_res_use();
}


Solver::~Solver()
{

}


void Solver::init_res_use()
{
	size_t n_res = this->inst.n_res;
	size_t n_trains = this->inst.n_trains();

	this->res_use.resize(n_trains);
	this->res_use_data.resize(n_res*n_trains, Res_use());

	for (auto t : this->inst.trains_range()) {
		this->res_use[t] = &this->res_use_data[t*n_res];
	}
}


void Solver::add_train(idx_t t)
{
	this->add_train_req_ops(t);
	this->add_train_level_dur(t);


	size_t state = STATE_UPDATE_GRAPH;

	while (state != STATE_DONE) {
		switch (state)	{
		  case STATE_SOLVE_MODEL:
			this->solve_model();
		    if (this->update_values()) {
				state = STATE_UPDATE_GRAPH;
			}
			else {
				state = STATE_UPDATE_OBJ;
			}
			break;
		
		  case STATE_UPDATE_GRAPH:
			this->update_graph();
			if (this->event_graph.update()) { // cycle found
				this->add_cycle_cons();
				state = STATE_SOLVE_MODEL;
			}
			else {
				state = STATE_UPDATE_OBJ;
			}
			break;

		  case STATE_UPDATE_OBJ:
			if (this->add_obj_cons()) {
				state = STATE_SOLVE_MODEL;
			}
			else {
				state = STATE_FIND_CONFLICT;
			}
			break;

		  case STATE_FIND_CONFLICT:
			if (this->add_conflict()) {
				state = STATE_UPDATE_GRAPH;
			}
			else {
				cout << "optimal sol found" << endl;
				state = STATE_DONE;
			}
			break;
		
		  default:
			assert(state == STATE_DONE);
			break;
		}
	}
}


void Solver::add_train_req_ops(idx_t t)
{
	for (auto& level : this->prepr.trains[t].levels) {
		if (level.n_succ() != 0) {
			continue;
		} 
		idx_t o = level.succ[0].op;

		auto& op = this->inst.ops[o];
		auto& l_op = this->prepr.op_level[o];
		assert(l_op.start == level.idx && l_op.end == level.idx + 1);

		this->level_dur[level.idx] = op.dur;
		
		if (op.n_res() > 0) {
			assert(op.n_res() == 1);
			auto& res = op.res[0];
			this->level_res[level.idx] = res.idx;

			auto& ru = this->res_use[t][res.idx];
			ru.level = l_op;
			ru.time = res.time;
		}

		if (op.obj < IDX_MAX) {
			this->add_obj(level.idx, op.obj);
		}

		this->event_graph.time_lb[level.idx] = op.start_lb;
	}
}


void Solver::add_train_level_dur(idx_t t)
{
	for (auto& level : this->prepr.trains[t].levels) {
		if (level.n_succ() == 0 || this->level_dur[level.idx] < DUR_MAX) {
			continue;
		}

		dur_t dur = DUR_MAX;
		for (auto& succ : level.succ) {
			idx_t o = succ.op;
			
			auto& op = this->inst.ops[o];
			auto& l_op = this->prepr.op_level[o];
			assert(l_op.start == level.idx && l_op.end == level.idx + 1);

			dur = MIN(dur, op.dur);
		}

		this->level_dur[level.idx] = dur;
	}
}



void Solver::add_obj(idx_t level, idx_t inst_idx)
{
	auto& inst_obj = this->inst.objs[inst_idx];

	Obj obj = {
		.idx = (idx_t)this->objs.size(),
		.level = level,
		.is_bin = inst_obj.increment > 0,
		.threshold = inst_obj.threshold
	};

	GRBVar var;
	if (obj.is_bin) {
		var = this->model.addVar(0, 1, inst_obj.increment, GRB_BINARY);
	}
	else {
		var = this->model.addVar(0, GRB_INFINITY, inst_obj.coeff, GRB_CONTINUOUS);
	}

	this->objs.push_back(obj);
	this->obj_vars.push_back(var);
	this->obj_values.push_back(0);
}


void Solver::update_graph()
{
	this->event_graph.clear_edges();
	this->add_dur_edges();
	this->add_conf_edges();
}


void Solver::add_dur_edges()
{
	for (auto l : prepr.levels_range()) {
		dur_t dur = this->level_dur[l];
		if (dur < DUR_MAX) {
			idx_t l_next = l + 1;
			this->event_graph.add_edge({IDX_MAX, l, l_next, dur});
		}
	}
}


void Solver::add_conf_edges()
{
	for (auto& conf : this->conflicts) {
		auto& ru1 = this->res_use[conf.train.first][conf.res.first];
		auto& ru2 = this->res_use[conf.train.second][conf.res.second];

		bool order = (conf.var == IDX_MAX) ? conf.freeze : this->conf_values[conf.var];
		if (order) {

			this->event_graph.add_edge({conf.var, ru1.level.end, ru2.level.start, ru1.time});
		}
	}
}

void Solver::solve_model()
{
	this->model.optimize();
	if (this->model.get(GRB_IntAttr_Status) != GRB_OPTIMAL) {
		cout << "ERROR: model infeasible" << endl;
		exit(1);
	}

	this->last_obj_val = this->model.get(GRB_DoubleAttr_Obj);
}


bool Solver::update_values()
{
	bool update_needed = false;

	size_t n_vars = this->conf_vars.size();
	for (size_t i = 0; i < n_vars; i++) {
		auto& var = this->conf_vars[i];
		
		bool old_val = this->conf_values[i];
		bool new_val = var.get(GRB_DoubleAttr_X) > 0.5;
		this->conf_values[i] = new_val;
		
		if (old_val != new_val) {
			update_needed = true;
		}
	}

	n_vars = this->obj_vars.size();
	for (size_t i = 0; i < n_vars; i++) {
		auto& var = this->obj_vars[i];
		this->obj_values[i] = round(var.get(GRB_DoubleAttr_X));
	}

	return update_needed;
}

vector<Solver::Var_assign> Solver::collect_assigns(const vector<Event_graph::Edge_vertex>& path)
{
	this->assign_set.clear();
	for (auto& x : path) {
		if (x.e < IDX_MAX) {
			assert(x.e < this->conf_values.size());
			this->assign_set.insert(x.e);
		}
	}

	vector<Solver::Var_assign> assigns = {};
	for (auto& x : this->assign_set) {
		assigns.push_back({x, this->conf_values[x]});
	}

	return assigns;
}


void Solver::add_cycle_cons()
{
	auto& cycle = this->event_graph.get_shortest_cycle();
	
	Cycle_cons cons;

	cons.assigns = this->collect_assigns(cycle);
	assert(cons.assigns.size() >= 2);

	cons.add_to_model(this->model, this->conf_vars);

	this->cycle_cons.push_back(cons);
}


bool Solver::add_obj_cons()
{
	tim_t max_diff = 0;
	tim_t max_delay = 0;
	idx_t max_idx = IDX_MAX;

	for (auto& obj : this->objs) {
		tim_t value = this->obj_values[obj.idx];
		if (obj.is_bin && value > 0) {
			continue;
		}

		tim_t delay = this->event_graph.time(obj.level) - obj.threshold;
		if (!obj.is_bin && value >= delay) {
			continue;
		}

		tim_t diff = delay - value;
		if (max_diff < diff) {
			max_diff = diff;
			max_delay = delay;
			max_idx = obj.idx;
		}
	}

	if (max_idx == IDX_MAX) {
		return false;
	}

	auto& obj = this->objs[max_idx];

	auto& path = this->event_graph.get_critical_path(obj.level);

	Path_cons cons;
	cons.is_bin = obj.is_bin;
	cons.obj_idx = obj.idx;
	cons.delay = max_delay;
	cons.assigns = this->collect_assigns(path);

	cons.add_to_model(this->model, this->conf_vars, this->obj_vars);
	this->path_cons.push_back(cons);

	return true;
}


bool Solver::add_conflict()
{
	Conflict conf;

	return false;
}


void Solver::Cycle_cons::add_to_model(GRBModel& model, 
	const vector<GRBVar>& conf_vars)
{
	if (this->in_model) {
		return;
	}
	
	GRBLinExpr expr(0);
	for (auto x : this->assigns) {
		expr += x.to_expr(conf_vars);
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


void Solver::Path_cons::add_to_model(GRBModel& model, 
	const vector<GRBVar>& conf_vars, 
	const vector<GRBVar>& obj_vars)
{
	if (this->in_model) {
		return;
	}
	
	GRBLinExpr expr(0);
	for (auto x : this->assigns) {
		expr += x.to_expr(conf_vars);
	}

	auto& obj_var = obj_vars[this->obj_idx];
	if (this->is_bin) {
		this->model_cons = model.addConstr(expr <= obj_var);
	}
	else {
		this->model_cons = model.addConstr(expr*this->delay <= obj_var);
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

