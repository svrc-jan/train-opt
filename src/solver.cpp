#include "solver.hpp"

#include <cmath>
#include "utils/stl_print.hpp"

using namespace std;


Solver::Solver(const Preprocess& prepr, GRBEnv& grb_env)
	: inst(prepr.inst), prepr(prepr), grb_env(grb_env), model(grb_env)
{
	size_t n_levels = this->prepr.n_levels();
	size_t n_trains = this->inst.n_trains();

	this->event_graph.set_n_vtx(n_levels);

	this->objs.reserve(n_trains);

	this->level_dur.resize(n_levels, IDX_MAX);
	
	this->res_ints.resize(this->inst.n_res, {});
	
	this->init_res_use();

	// this->model.set(GRB_IntParam_Presolve, 0);
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


void Solver::solve()
{
	for (auto t : this->inst.trains_range()) {
		this->add_train(t);
	}
}


void Solver::add_train(idx_t t)
{
	this->add_train_req_ops(t);
	this->add_train_level_dur(t);
	this->add_train_req_objs(t);

	cout << "add train " << t << endl;

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

	this->freeze_conflicts();
	this->clear_model();
}



void Solver::add_train_route(idx_t t)
{

}


void Solver::add_train_req_ops(idx_t t)
{
	for (auto& level : this->prepr.trains[t].levels) {
		if (level.n_succ() != 1) {
			continue;
		}
		
		idx_t o = level.succ[0].op;

		auto& op = this->inst.ops[o];
		auto& l_op = this->prepr.ops[o].level;
		assert(l_op.start == level.idx && l_op.end == level.idx + 1);

		this->level_dur[level.idx] = op.dur;
		
		if (op.n_res() > 0) {
			assert(op.n_res() == 1);
			auto& res = op.res[0];

			auto& ru = this->res_use[t][res.idx];
			ru.level = l_op;
			ru.time = res.time;
		}

		this->event_graph.time_lb[level.idx] = op.start_lb;
	}
}


void Solver::add_train_req_objs(idx_t t)
{
	for (auto& level : this->prepr.trains[t].levels) {
		if (level.n_succ() != 1) {
			continue;
		}
		
		idx_t o = level.succ[0].op;
		auto& op = this->inst.ops[o];
		
		if (op.obj < IDX_MAX) {
			this->add_obj(level.train, level.idx, op.obj);
		}
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
			auto& l_op = this->prepr.ops[o].level;
			assert(l_op.start == level.idx && l_op.end == level.idx + 1);

			dur = MIN(dur, op.dur);
		}

		this->level_dur[level.idx] = dur;
	}
}



void Solver::add_obj(idx_t train, idx_t level, idx_t inst_idx)
{
	auto& inst_obj = this->inst.objs[inst_idx];

	Obj obj = {
		.idx = (idx_t)this->objs.size(),
		.train = train,
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
	cout << "update graph" << endl;
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
			this->event_graph.add_edge({{l, l_next}, IDX_MAX, dur});
		}
	}
}


void Solver::add_conf_edges()
{
	for (auto& conf : this->conflicts) {
		auto& ru1 = this->res_use[conf.train.first][conf.res];
		auto& ru2 = this->res_use[conf.train.second][conf.res];

		bool order = (conf.var == IDX_MAX) ? conf.freeze : this->conf_values[conf.var];
		if (order) {
			this->event_graph.add_edge({{ru1.level.end, ru2.level.start}, conf.var, ru1.time});
		}
		else {
			this->event_graph.add_edge({{ru2.level.end, ru1.level.start}, conf.var, ru2.time});
		}
	}
}


void Solver::clear_model()
{
	cout << "clear model" << endl;
	for (auto& x : this->cycle_cons) {
		x.remove_from_model(this->model);
	}
	this->cycle_cons.clear();

	for (auto& x : this->path_cons) {
		x.remove_from_model(this->model);
	}
	this->path_cons.clear();
	
	for (auto& x : this->conf_vars) {
		this->model.remove(x);
	}
	this->conf_vars.clear();
	this->conf_values.clear();
}

void Solver::solve_model()
{
	cout << "solve model" << endl;

	int status;
	try {
		this->model.update();
		this->model.optimize();
		status = this->model.get(GRB_IntAttr_Status);
		if (status == GRB_OPTIMAL) {
			this->last_obj_val = this->model.get(GRB_DoubleAttr_ObjVal);
		}
		
	}
	catch (const GRBException& ex) {
		cout << "ERROR: optimization exception: " << ex.getMessage() << ", code: " << ex.getErrorCode() << endl;
		exit(1);
	}

	if (status != GRB_OPTIMAL) {
		cout << "ERROR: model optim failed, status: " << status << endl;
		exit(1);
	}
}


bool Solver::update_values()
{
	bool update_needed = false;

	cout << "update values = ";

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

	cout << (update_needed ? "true" : "false") << endl;

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
	cout << "add cycle cons ";

	auto& cycle = this->event_graph.get_shortest_cycle();
	
	Cycle_cons cons;

	cons.assigns = this->collect_assigns(cycle);
	cout << cons.assigns << endl;

	
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

	cout << "add obj cons t" << obj.train;
	if (obj.is_bin) {
		cout << ", bin, ";
	}
	else {
		cout << ", delay " << max_delay << ", ";
	}

	Path_cons cons;
	cons.is_bin = obj.is_bin;
	cons.obj_idx = obj.idx;
	cons.delay = max_delay;
	cons.assigns = this->collect_assigns(path);

	cout << cons.assigns << endl;

	cons.add_to_model(this->model, this->conf_vars, this->obj_vars);
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

	size_t n_assigns = this->assigns.size();

	auto& obj_var = obj_vars[this->obj_idx];
	if (this->is_bin) {
		this->model_cons = model.addConstr((expr - n_assigns + 1) <= obj_var);
	}
	else {
		this->model_cons = model.addConstr((expr - n_assigns + 1)*this->delay <= obj_var);
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

