#include "path_select.hpp"


using namespace std;


Path_select::Path_select(const Preprocess& prepr, const GRBEnv& grb_env)
	: inst(prepr.inst), prepr(prepr), grb_env(grb_env)
{
	
}


Path_select::~Path_select()
{
	if (this->op_var != nullptr) {
		delete this->op_var;
	}

	if (this->model != nullptr) {
		delete this->model;
	}
}


void Path_select::select_paths(vector<vector<int>>& paths, vector<int>& level_time)
{
	if (this->model == nullptr) {
		this->init_model();
	}
	else {
		this->clear_model();
	}

	this->propagate_level_time(level_time);
	
	vector<Res_overlap> overlaps;
	this->get_res_overlaps(overlaps, level_time);
	this->add_overlaps_to_model(overlaps);

	this->model->optimize();

	this->extract_paths_from_sol(paths);
}

void Path_select::select_paths(vector<vector<int>>& paths)
{
	vector<int> level_time(this->prepr.n_levels(), -1);
	this->select_paths(paths, level_time);
}


void Path_select::init_model()
{
	this->model = new GRBModel(this->grb_env);
	this->make_op_var();
	this->make_flow_cons();
}



void Path_select::make_op_var()
{
	this->op_var = this->model->addVars(this->inst.n_ops(), GRB_BINARY);
}


void Path_select::make_flow_cons()
{
	for (auto& level : this->prepr.levels) {
		GRBLinExpr var_sum = 0;
		
		if (level.ops_in.size == 0) {
			var_sum += 1;
		}
		else {
			for (int o : level.ops_in) {
				var_sum += this->op_var[o];
			}
		}

		if (level.ops_out.size == 0) {
			var_sum -= 1;
		}
		else {
			for (int o : level.ops_out) {
				var_sum -= this->op_var[o];
			}
		}

		this->model->addConstr(var_sum == 0);
	}
}


void Path_select::clear_model()
{
	for (auto& c : this->res_cons) {
		this->model->remove(c);
	}
	this->res_cons.clear();

	for (auto& v : this->res_var) {
		this->model->remove(v);
	}
	this->res_var.clear();
}


void Path_select::get_res_overlaps(vector<Res_overlap>& overlaps, 
	const vector<int>& level_time)
{
	vector<vector<Res_interval>> res_ints;
	res_ints.resize(this->inst.n_res(), {});

	for (int o = 0; o < this->inst.n_ops(); o++) {
		auto& op_level = this->prepr.op_level[o];

		int start = level_time[op_level.first];
		int end = level_time[op_level.second];

		for (auto& res : this->inst.ops[o].res) {
			res_ints[res.idx].push_back(Res_interval(o, start, end + res.time));
		}
	}

	overlaps.clear();
	for (auto& ri : res_ints) {
		sort(ri.begin(), ri.end());

		for (int i = 0; i < (int)ri.size(); i++) {
			auto& a = ri[i];
			for (int j = i+1; j < (int)ri.size(); j++) {
				auto& b = ri[j];
				
				int size = a.start - b.end;

				if (size > 0) {
					overlaps.push_back(Res_overlap(a.op, b.op, size));
				}
				else {
					break;
				}
			}
		}
	}

	sort(overlaps.begin(), overlaps.end());

	int i_max = (int)overlaps.size() - 1;
	for (int i = 0; i <= i_max; i++) {
		auto& a = overlaps[i];
		auto& b = overlaps[i + 1];
		
		if (i < i_max && a.op1 == b.op1 && a.op2 == b.op2) {
			b.size += a.size;
			a.size = 0;
		}
	} 
}


void Path_select::add_overlaps_to_model(const vector<Res_overlap>& overlaps)
{
	for (auto& ol : overlaps) {
		if (ol.size == 0) {
			continue;
		}

		GRBVar var = this->model->addVar(0.0, GRB_INFINITY, ol.size, GRB_CONTINUOUS);
		
		GRBTempConstr tmp_cons = this->op_var[ol.op1] + this->op_var[ol.op2] <= var + 1;
		GRBConstr cons = this->model->addConstr(tmp_cons);
		
		this->res_var.push_back(var);
		this->res_cons.push_back(cons);
	}
}


void Path_select::propagate_level_time(vector<int>& level_time)
{
	for (int l = 0; l < this->prepr.n_levels(); l++) {
		auto& level = this->prepr.levels[l];

		if (level_time[l] < 0) {
			level_time[l] = level.time_lb;
			
			int path_time = INT_MAX;
			
			for (int o : level.ops_in) {
				int dur = this->inst.ops[o].dur;
				int l_pred = this->prepr.op_level_start(o);

				path_time = min(path_time, level_time[l_pred] + dur);
			}

			if (path_time < INT_MAX) {
				level_time[l] = max(level_time[l], path_time);
			}
		}
	}
}


void Path_select::extract_paths_from_sol(std::vector<std::vector<int>>& paths)
{
	if ((int)paths.size() != this->inst.n_trains()) {
		paths.resize(this->inst.n_trains(), {});
	}

	for (int t = 0; t < this->inst.n_trains(); t++) {
		auto& path = paths[t];
		auto& train = this->inst.trains[t];

		path.clear();
		path.push_back(train.op_start);

		while (path.back() != train.op_end()) {
			auto& op = this->inst.ops[path.back()];

			int o_select = -1;
			for (int o_succ : op.succ) {
				if (this->op_var[o_succ].get(GRB_DoubleAttr_X) > 0.5) {
					assert(o_select == -1);
					o_select = o_succ;
				}
			}

			path.push_back(o_select);
		}
	}
}

