#include "path_select.hpp"

#include "utils/stl_print.hpp"

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


void Path_select::select_paths(vector<vector<int>>& paths, vector<int>& junct_time)
{
	this->propagate_junct_time(junct_time);
	
	vector<Res_overlap> overlaps;
	this->get_res_overlaps(overlaps, junct_time);

	int n_ops = this->inst.n_ops();
	int n_trains = this->inst.n_trains();
	
	vector<bool> train_selected(n_trains, false);
	vector<int> op_overlap(n_ops);

	while (true) {
		
	}
}


void Path_select::select_paths(vector<vector<int>>& paths)
{
	vector<int> junct_time(this->prepr.n_juncts(), -1);
	this->select_paths(paths, junct_time);
}


void Path_select::get_res_overlaps(vector<Res_overlap>& overlaps, 
	const vector<int>& junct_time)
{
	vector<vector<Res_interval>> res_ints;
	res_ints.resize(this->inst.n_res(), {});

	for (int o = 0; o < this->inst.n_ops(); o++) {
		auto& i_op = this->inst.ops[o];
		auto& p_op = this->prepr.ops[o];
		
		int op_start = junct_time[p_op.junct_start];
		int op_end = junct_time[p_op.junct_end];

		for (auto& res : this->inst.ops[o].res) {
			int start = op_start - i_op.dur;
			int end = op_end + res.time + i_op.dur;
			res_ints[res.idx].push_back(Res_interval(i_op.train, o, start, end));
		}
	}

	overlaps.clear();
	for (auto& ri : res_ints) {
		sort(ri.begin(), ri.end());

		for (int i = 0; i < (int)ri.size(); i++) {
			auto& a = ri[i];
			for (int j = i+1; j < (int)ri.size(); j++) {
				auto& b = ri[j];
				
				int size = a.end - b.start;

				if (size > 0 && a.train != b.train) {
					overlaps.push_back(Res_overlap(a.op, b.op, size));
				}

				if (size <= 0) {
					break;
				}
			}
		}
	}

	sort(overlaps.begin(), overlaps.end());

	int new_size = overlaps.size();
	int i_max = (int)overlaps.size() - 1;
	for (int i = 0; i <= i_max; i++) {
		auto& a = overlaps[i];
		auto& b = overlaps[i + 1];
		
		if (i < i_max && a.op1 == b.op1 && a.op2 == b.op2) {
			b.size += a.size;
			a.size = 0;

			new_size -= 1;
		}
	}

	vector<Res_overlap> new_overlaps;
	new_overlaps.reserve(new_size);

	for (auto& ol : overlaps) {
		if (ol.size > 0) {
			new_overlaps.push_back(ol);
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


void Path_select::propagate_junct_time(vector<int>& junct_time)
{
	for (int l = 0; l < this->prepr.n_juncts(); l++) {
		auto& junct = this->prepr.juncts[l];

		if (junct_time[l] < 0) {
			junct_time[l] = junct.time_lb;
			
			int path_time = INT_MAX;
			
			for (int o : junct.ops_in) {
				int dur = this->inst.ops[o].dur;
				int j_pred = this->prepr.ops[o].junct_start;

				path_time = min(path_time, junct_time[j_pred] + dur);
			}

			if (path_time < INT_MAX) {
				junct_time[l] = max(junct_time[l], path_time);
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

		while (path.back() != train.op_last()) {
			int o = path.back();
			auto& op = this->inst.ops[o];

			int o_select = -1;
			for (int o_succ : op.succ) {
				if (this->op_var[o_succ].get(GRB_DoubleAttr_X) > 0.5) {
					assert(o_select == -1);
					o_select = o_succ;
				}
			}

			assert(o_select > 0);
			path.push_back(o_select);
		}
	}
}



void Path_select::Res_interval::print(std::ostream& os) const
{
	os << "Res_interval(op=" << this->op <<
		", start=" << this->start <<
		", end=" << this->end << ")";
}